// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/input_stream_impl.h"

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/c/system/data_pipe.h"

namespace nearby::chrome {

namespace {

void LogReadResult(connections::mojom::Medium medium, bool success) {
  switch (medium) {
    case connections::mojom::Medium::kBluetooth:
      base::UmaHistogramBoolean(
          "Nearby.Connections.Bluetooth.Socket.Read.Result", success);
      break;
    case connections::mojom::Medium::kWifiLan:
      base::UmaHistogramBoolean("Nearby.Connections.WifiLan.Socket.Read.Result",
                                success);
      break;
    case connections::mojom::Medium::kUnknown:
    case connections::mojom::Medium::kMdns:
    case connections::mojom::Medium::kWifiHotspot:
    case connections::mojom::Medium::kBle:
    case connections::mojom::Medium::kWifiAware:
    case connections::mojom::Medium::kNfc:
    case connections::mojom::Medium::kWifiDirect:
    case connections::mojom::Medium::kWebRtc:
    case connections::mojom::Medium::kBleL2Cap:
    case connections::mojom::Medium::kUsb:
    case connections::mojom::Medium::kWebRtcNonCellular:
      break;
  }
}

}  // namespace

InputStreamImpl::InputStreamImpl(
    connections::mojom::Medium medium,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::ScopedDataPipeConsumerHandle receive_stream)
    : medium_(medium),
      task_runner_(std::move(task_runner)),
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

InputStreamImpl::~InputStreamImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Close();
}

ExceptionOr<ByteArray> InputStreamImpl::Read(std::int64_t size) {
  if (IsClosed())
    return {Exception::kIo};

  bool invalid_size = size <= 0 || size > std::numeric_limits<uint32_t>::max();
  if (invalid_size) {
    // Only log it as a read failure when |size| is out of range as in
    // reality a null |receive_stream_| is an expected state after Close()
    // was called normally.
    LogReadResult(medium_, false);

    return {Exception::kIo};
  }

  pending_read_buffer_ = std::make_unique<ByteArray>(size);
  pending_read_buffer_pos_ = 0;

  // Signal and reset the WaitableEvent in case another thread is already
  // waiting on a Read().
  read_waitable_event_.Signal();
  read_waitable_event_.Reset();

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&mojo::SimpleWatcher::ArmOrNotify,
                                base::Unretained(&receive_stream_watcher_)));
  read_waitable_event_.Wait();

  pending_read_buffer_.reset();
  pending_read_buffer_pos_ = 0;

  // |receive_stream_| might have been reset in Close() while
  // |read_waitable_event_| was waiting.
  if (IsClosed())
    return {Exception::kIo};

  LogReadResult(medium_, exception_or_received_byte_array_.ok());
  return exception_or_received_byte_array_;
}

Exception InputStreamImpl::Close() {
  // NOTE(http://crbug.com/1247876): Close() might be called from multiple
  // threads at the same time; sequence calls and check if stream is already
  // closed inside task. Also, must cancel |receive_stream_watcher_| on the
  // same sequence it was initialized on.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    // No need to post a task if this is already running on |task_runner_|.
    DoClose(/*task_run_waitable_event=*/nullptr);
  } else {
    base::WaitableEvent task_run_waitable_event;
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&InputStreamImpl::DoClose,
                                                     base::Unretained(this),
                                                     &task_run_waitable_event));
    task_run_waitable_event.Wait();
  }

  return {Exception::kSuccess};
}

void InputStreamImpl::ReceiveMore(MojoResult result,
                                  const mojo::HandleSignalsState& state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(result, MOJO_RESULT_SHOULD_WAIT);
  DCHECK(!IsClosed());
  DCHECK(pending_read_buffer_);
  DCHECK_LT(pending_read_buffer_pos_, pending_read_buffer_->size());

  if (state.peer_closed()) {
    exception_or_received_byte_array_ = ExceptionOr<ByteArray>(Exception::kIo);
    read_waitable_event_.Signal();
    return;
  }

  if (result == MOJO_RESULT_OK) {
    base::span<uint8_t> buffer =
        base::as_writable_byte_span(*pending_read_buffer_)
            .subspan(pending_read_buffer_pos_);
    size_t bytes_read = 0;
    result =
        receive_stream_->ReadData(MOJO_READ_DATA_FLAG_NONE, buffer, bytes_read);
    if (result == MOJO_RESULT_OK) {
      pending_read_buffer_pos_ += bytes_read;
    }
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
    exception_or_received_byte_array_ = ExceptionOr<ByteArray>(Exception::kIo);
  }
  read_waitable_event_.Signal();
}

bool InputStreamImpl::IsClosed() const {
  return !receive_stream_.is_valid();
}

void InputStreamImpl::DoClose(base::WaitableEvent* task_run_waitable_event) {
  // Must cancel |receive_stream_watcher_| on the same sequence it was
  // initialized on.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!IsClosed()) {
    receive_stream_watcher_.Cancel();
    receive_stream_.reset();

    // It is possible that a Read() call could still be blocking a different
    // sequence via |read_waitable_event_| when Close() is called. If we only
    // cancel the stream watcher, the Read() call will block forever. We
    // trigger the event manually here, which will cause an IO exception to be
    // returned from Read().
    read_waitable_event_.Signal();
  }

  if (task_run_waitable_event)
    task_run_waitable_event->Signal();
}

}  // namespace nearby::chrome
