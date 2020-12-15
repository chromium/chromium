// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_socket.h"

#include <stdint.h>
#include <limits>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/nearby/src/cpp/platform/public/count_down_latch.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {

void LogSocketReadResult(bool success) {
  base::UmaHistogramBoolean("Nearby.Connections.Bluetooth.Socket.Read.Result",
                            success);
}

void LogSocketWriteResult(bool success) {
  base::UmaHistogramBoolean("Nearby.Connections.Bluetooth.Socket.Write.Result",
                            success);
}

// Concrete InputStream implementation, tightly coupled to BluetoothSocket.
class InputStreamImpl : public InputStream {
 public:
  InputStreamImpl(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  mojo::ScopedDataPipeConsumerHandle receive_stream)
      : task_runner_(std::move(task_runner)),
        receive_stream_(std::move(receive_stream)),
        receive_stream_watcher_(FROM_HERE,
                                mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                                task_runner_) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    receive_stream_watcher_.Watch(
        receive_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&InputStreamImpl::ReceiveMore,
                            base::Unretained(this)));
  }

  ~InputStreamImpl() override { this->Close(); }

  InputStreamImpl(const InputStreamImpl&) = delete;
  InputStreamImpl& operator=(const InputStreamImpl&) = delete;

  // InputStream:
  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    bool invalid_size =
        size <= 0 || size > std::numeric_limits<uint32_t>::max();
    if (!receive_stream_ || invalid_size) {
      if (invalid_size) {
        // Only log it as a read failure when |size| is out of range as in
        // reality a null |receive_stream_| is an expected state after Close()
        // was called normally.
        LogSocketReadResult(false);
      }
      return {Exception::kIo};
    }

    pending_read_buffer_ = std::make_unique<ByteArray>(size);
    pending_read_buffer_pos_ = 0;

    read_waitable_event_.emplace();
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&mojo::SimpleWatcher::ArmOrNotify,
                                  base::Unretained(&receive_stream_watcher_)));
    read_waitable_event_->Wait();
    read_waitable_event_.reset();

    pending_read_buffer_.reset();
    pending_read_buffer_pos_ = 0;

    // |receive_stream_| might have been reset in Close() while
    // |read_waitable_event_| was waiting.
    if (!receive_stream_)
      return {Exception::kIo};

    LogSocketReadResult(exception_or_received_byte_array_.ok());
    return exception_or_received_byte_array_;
  }
  Exception Close() override {
    if (!receive_stream_)
      return {Exception::kSuccess};

    // Must cancel |receive_stream_watcher_| on the same sequence it was
    // initialized on.
    base::WaitableEvent task_run;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(
                               [](mojo::SimpleWatcher* receive_stream_watcher,
                                  base::WaitableEvent* task_run) {
                                 receive_stream_watcher->Cancel();
                                 task_run->Signal();
                               },
                               &receive_stream_watcher_, &task_run));
    task_run.Wait();

    receive_stream_.reset();

    // It is possible that a Read() call could still be blocking a different
    // sequence via |read_waitable_event_| when Close() is called. Notably, this
    // happens on the receiving device when a Nearby Share transfer finishes. If
    // we only cancel the stream watcher, the Read() call will block forever. We
    // trigger the event manually here, which will cause an IO exception to be
    // returned from Read().
    if (read_waitable_event_)
      read_waitable_event_->Signal();

    return {Exception::kSuccess};
  }

 private:
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK_NE(result, MOJO_RESULT_SHOULD_WAIT);
    DCHECK(receive_stream_.is_valid());
    DCHECK(pending_read_buffer_);
    DCHECK_LT(pending_read_buffer_pos_, pending_read_buffer_->size());
    DCHECK(read_waitable_event_);

    if (state.peer_closed()) {
      exception_or_received_byte_array_ =
          ExceptionOr<ByteArray>(Exception::kIo);
      read_waitable_event_->Signal();
      return;
    }

    if (result == MOJO_RESULT_OK) {
      uint32_t num_bytes = static_cast<uint32_t>(pending_read_buffer_->size() -
                                                 pending_read_buffer_pos_);
      result = receive_stream_->ReadData(
          pending_read_buffer_->data() + pending_read_buffer_pos_, &num_bytes,
          MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_OK)
        pending_read_buffer_pos_ += num_bytes;
    }

    if (result == MOJO_RESULT_SHOULD_WAIT ||
        pending_read_buffer_pos_ < pending_read_buffer_->size()) {
      receive_stream_watcher_.ArmOrNotify();
      return;
    }

    if (result == MOJO_RESULT_OK) {
      exception_or_received_byte_array_ =
          ExceptionOr<ByteArray>(std::move(*pending_read_buffer_));
    } else {
      exception_or_received_byte_array_ =
          ExceptionOr<ByteArray>(Exception::kIo);
    }
    read_waitable_event_->Signal();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::SimpleWatcher receive_stream_watcher_;

  std::unique_ptr<ByteArray> pending_read_buffer_;
  uint32_t pending_read_buffer_pos_ = 0;
  ExceptionOr<ByteArray> exception_or_received_byte_array_;
  base::Optional<base::WaitableEvent> read_waitable_event_;
};

// Concrete OutputStream implementation, tightly coupled to BluetoothSocket.
class OutputStreamImpl : public OutputStream {
 public:
  OutputStreamImpl(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   mojo::ScopedDataPipeProducerHandle send_stream)
      : task_runner_(std::move(task_runner)),
        send_stream_(std::move(send_stream)),
        send_stream_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             task_runner_) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    send_stream_watcher_.Watch(
        send_stream_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&OutputStreamImpl::SendMore,
                            base::Unretained(this)));
  }

  ~OutputStreamImpl() override { this->Close(); }

  OutputStreamImpl(const OutputStreamImpl&) = delete;
  OutputStreamImpl& operator=(const OutputStreamImpl&) = delete;

  // OutputStream:
  Exception Write(const ByteArray& data) override {
    if (!send_stream_)
      return {Exception::kIo};

    DCHECK(!write_success_);
    pending_write_buffer_ = std::make_unique<ByteArray>(data);
    pending_write_buffer_pos_ = 0;

    write_waitable_event_.emplace();
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&mojo::SimpleWatcher::ArmOrNotify,
                                  base::Unretained(&send_stream_watcher_)));
    write_waitable_event_->Wait();

    Exception result = {write_success_ ? Exception::kSuccess : Exception::kIo};

    write_success_ = false;
    pending_write_buffer_.reset();
    pending_write_buffer_pos_ = 0;
    write_waitable_event_.reset();

    // |send_stream_| might have been reset in Close() while
    // |write_waitable_event_| was waiting.
    if (!send_stream_)
      return {Exception::kIo};

    // Ignore a null |send_stream_| when logging since it might be an expected
    // state as mentioned above.
    LogSocketWriteResult(result.Ok());
    return result;
  }
  Exception Flush() override {
    // TODO(hansberry): Unsure if anything can reasonably be done here. Need to
    // ask a reviewer from the Nearby team.
    return {Exception::kSuccess};
  }
  Exception Close() override {
    if (!send_stream_)
      return {Exception::kSuccess};

    // Must cancel |send_stream_watcher_| on the same sequence it was
    // initialized on.
    base::WaitableEvent task_run;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(
                               [](mojo::SimpleWatcher* send_stream_watcher,
                                  base::WaitableEvent* task_run) {
                                 send_stream_watcher->Cancel();
                                 task_run->Signal();
                               },
                               &send_stream_watcher_, &task_run));
    task_run.Wait();

    send_stream_.reset();

    // It is possible that a Write() call could still be blocking a different
    // sequence via |write_waitable_event_| when Close() is called. If we only
    // cancel the stream watcher, the Write() call will block forever. We
    // trigger the event manually here, which will cause an IO exception to be
    // returned from Write().
    if (write_waitable_event_)
      write_waitable_event_->Signal();

    return {Exception::kSuccess};
  }

 private:
  void SendMore(MojoResult result, const mojo::HandleSignalsState& state) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK_NE(result, MOJO_RESULT_SHOULD_WAIT);
    DCHECK(send_stream_.is_valid());
    DCHECK(pending_write_buffer_);
    DCHECK_LT(pending_write_buffer_pos_, pending_write_buffer_->size());
    DCHECK(write_waitable_event_);

    if (state.peer_closed()) {
      write_success_ = false;
      write_waitable_event_->Signal();
      return;
    }

    if (result == MOJO_RESULT_OK) {
      uint32_t num_bytes = static_cast<uint32_t>(pending_write_buffer_->size() -
                                                 pending_write_buffer_pos_);
      result = send_stream_->WriteData(
          pending_write_buffer_->data() + pending_write_buffer_pos_, &num_bytes,
          MOJO_WRITE_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_OK)
        pending_write_buffer_pos_ += num_bytes;
    }

    if (result == MOJO_RESULT_SHOULD_WAIT ||
        pending_write_buffer_pos_ < pending_write_buffer_->size()) {
      send_stream_watcher_.ArmOrNotify();
      return;
    }

    write_success_ = result == MOJO_RESULT_OK;
    write_waitable_event_->Signal();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  mojo::SimpleWatcher send_stream_watcher_;

  std::unique_ptr<ByteArray> pending_write_buffer_;
  uint32_t pending_write_buffer_pos_ = 0;
  bool write_success_ = false;
  base::Optional<base::WaitableEvent> write_waitable_event_;
};

}  // namespace

BluetoothSocket::BluetoothSocket(
    bluetooth::mojom::DeviceInfoPtr device,
    mojo::PendingRemote<bluetooth::mojom::Socket> socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream)
    : remote_device_(std::move(device)),
      remote_device_ref_(*remote_device_),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      socket_(std::move(socket), task_runner_) {
  InitializeStreams(std::move(receive_stream), std::move(send_stream));
}

BluetoothSocket::BluetoothSocket(
    api::BluetoothDevice& remote_device,
    mojo::PendingRemote<bluetooth::mojom::Socket> socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream)
    : remote_device_ref_(remote_device),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      socket_(std::move(socket), task_runner_) {
  InitializeStreams(std::move(receive_stream), std::move(send_stream));
}

BluetoothSocket::~BluetoothSocket() {
  Close();

  // These properties must be destroyed on the same sequence they are later run
  // on. See |task_runner_|.
  CountDownLatch latch(2);

  auto count_down_latch_callback = base::BindRepeating(
      [](CountDownLatch* latch) { latch->CountDown(); }, &latch);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocket::DestroyInputStream,
                     base::Unretained(this), count_down_latch_callback));
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocket::DestroyOutputStream,
                     base::Unretained(this), count_down_latch_callback));

  latch.Await();
}

InputStream& BluetoothSocket::GetInputStream() {
  DCHECK(input_stream_);
  return *input_stream_;
}

OutputStream& BluetoothSocket::GetOutputStream() {
  DCHECK(output_stream_);
  return *output_stream_;
}

Exception BluetoothSocket::Close() {
  if (socket_) {
    socket_->Disconnect();
    socket_.reset();
  }
  Exception input_exception = input_stream_->Close();
  Exception output_exception = output_stream_->Close();
  if (input_exception.Ok() && output_exception.Ok())
    return {Exception::kSuccess};
  if (!input_exception.Ok())
    return input_exception;
  if (!output_exception.Ok())
    return output_exception;
  NOTREACHED();
  return {Exception::kFailed};
}

api::BluetoothDevice* BluetoothSocket::GetRemoteDevice() {
  return &remote_device_ref_;
}

void BluetoothSocket::InitializeStreams(
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  // These properties must be created on the same sequence they are later run
  // on. See |task_runner_|.
  CountDownLatch latch(2);

  auto count_down_latch_callback = base::BindRepeating(
      [](CountDownLatch* latch) { latch->CountDown(); }, &latch);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocket::CreateInputStream,
                     base::Unretained(this), std::move(receive_stream),
                     count_down_latch_callback));
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothSocket::CreateOutputStream,
                                base::Unretained(this), std::move(send_stream),
                                count_down_latch_callback));

  latch.Await();
}

void BluetoothSocket::CreateInputStream(
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    base::OnceClosure callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  input_stream_ = std::make_unique<InputStreamImpl>(task_runner_,
                                                    std::move(receive_stream));
  std::move(callback).Run();
}

void BluetoothSocket::DestroyInputStream(base::OnceClosure callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  input_stream_.reset();
  std::move(callback).Run();
}

void BluetoothSocket::CreateOutputStream(
    mojo::ScopedDataPipeProducerHandle send_stream,
    base::OnceClosure callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  output_stream_ =
      std::make_unique<OutputStreamImpl>(task_runner_, std::move(send_stream));
  std::move(callback).Run();
}

void BluetoothSocket::DestroyOutputStream(base::OnceClosure callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  output_stream_.reset();
  std::move(callback).Run();
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
