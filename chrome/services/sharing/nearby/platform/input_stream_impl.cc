// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/input_stream_impl.h"

#include <limits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/stack_allocated.h"
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
    case connections::mojom::Medium::kAwdl:
      break;
  }
}

}  // namespace

class InputStreamImpl::ScopedInFlightRead {
  STACK_ALLOCATED();

 public:
  explicit ScopedInFlightRead(InputStreamImpl* stream) : stream_(stream) {}
  ~ScopedInFlightRead() { stream_->OnReadFinished(); }
  ScopedInFlightRead(const ScopedInFlightRead&) = delete;
  ScopedInFlightRead& operator=(const ScopedInFlightRead&) = delete;

 private:
  InputStreamImpl* const stream_;
};

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
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  Close();
  base::AutoLock lock(lock_);
  CHECK_EQ(reads_in_flight_, 0u);
}

ExceptionOr<ByteArray> InputStreamImpl::Read(std::int64_t size) {
  CHECK(!task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    if (is_closed_) {
      return {Exception::kIo};
    }
    ++reads_in_flight_;
  }
  ScopedInFlightRead scoped_in_flight(this);

  bool invalid_size = size <= 0 || size > std::numeric_limits<uint32_t>::max();
  if (invalid_size) {
    // Only log it as a read failure when |size| is out of range as in
    // reality a null |receive_stream_| is an expected state after Close()
    // was called normally.
    LogReadResult(medium_, false);

    return {Exception::kIo};
  }

  base::WaitableEvent read_waitable_event;
  ExceptionOr<ByteArray> result(Exception::kIo);
  bool closed_during_read = false;

  if (!task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&InputStreamImpl::StartRead, base::Unretained(this),
                         static_cast<size_t>(size), &read_waitable_event,
                         &result, &closed_during_read))) {
    return {Exception::kIo};
  }
  read_waitable_event.Wait();

  // Skip LogReadResult() when Close() interrupts a pending Read(), since that
  // is expected during normal teardown.
  if (closed_during_read) {
    return {Exception::kIo};
  }

  LogReadResult(medium_, result.ok());
  return result;
}

Exception InputStreamImpl::Close() {
  {
    base::AutoLock lock(lock_);
    is_closed_ = true;
  }

  // NOTE(http://crbug.com/40197037): Close() might be called from multiple
  // threads at the same time; sequence calls and check if stream is already
  // closed inside task. Also, must cancel |receive_stream_watcher_| on the
  // same sequence it was initialized on.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    // No need to post a task if this is already running on |task_runner_|.
    DoClose(/*task_run_waitable_event=*/nullptr);
  } else {
    base::WaitableEvent task_run_waitable_event;
    if (task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&InputStreamImpl::DoClose, base::Unretained(this),
                           &task_run_waitable_event))) {
      task_run_waitable_event.Wait();
    }
  }

  return {Exception::kSuccess};
}

void InputStreamImpl::WaitForInFlightReads() {
  CHECK(!task_runner_->RunsTasksInCurrentSequence());
  base::WaitableEvent drain_event;
  {
    base::AutoLock lock(lock_);
    CHECK(is_closed_);
    CHECK(!drain_waitable_event_);
    if (reads_in_flight_ == 0) {
      return;
    }
    drain_waitable_event_ = &drain_event;
  }
  drain_event.Wait();
}

void InputStreamImpl::SetOnReadPendingForTesting(base::OnceClosure callback) {
  base::AutoLock lock(lock_);
  on_read_pending_for_testing_ = std::move(callback);
}

void InputStreamImpl::StartRead(size_t size,
                                base::WaitableEvent* read_waitable_event,
                                ExceptionOr<ByteArray>* result,
                                bool* closed_during_read) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!receive_stream_.is_valid() || IsClosed()) {
    *closed_during_read = true;
    read_waitable_event->Signal();
    return;
  }

  // Wake any previously waiting Read() before replacing the active read state.
  if (pending_read_waitable_event_) {
    FinishPendingRead(ExceptionOr<ByteArray>(Exception::kIo), /*closed=*/false);
  }

  pending_read_buffer_ = std::make_unique<ByteArray>(size);
  pending_read_buffer_pos_ = 0;
  pending_read_waitable_event_ = read_waitable_event;
  pending_read_result_ = result;
  pending_read_closed_ = closed_during_read;

  receive_stream_watcher_.ArmOrNotify();

  base::OnceClosure on_read_pending;
  {
    base::AutoLock lock(lock_);
    on_read_pending = std::move(on_read_pending_for_testing_);
  }
  if (on_read_pending) {
    std::move(on_read_pending).Run();
  }
}

void InputStreamImpl::ReceiveMore(MojoResult result,
                                  const mojo::HandleSignalsState& state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(result, MOJO_RESULT_SHOULD_WAIT);
  DCHECK(receive_stream_.is_valid());
  if (!pending_read_waitable_event_) {
    return;
  }
  DCHECK(pending_read_buffer_);
  DCHECK_LT(pending_read_buffer_pos_, pending_read_buffer_->size());

  if (state.peer_closed()) {
    FinishPendingRead(ExceptionOr<ByteArray>(Exception::kIo), /*closed=*/false);
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
    FinishPendingRead(ExceptionOr<ByteArray>(std::move(*pending_read_buffer_)),
                      /*closed=*/false);
  } else {
    FinishPendingRead(ExceptionOr<ByteArray>(Exception::kIo), /*closed=*/false);
  }
}

void InputStreamImpl::FinishPendingRead(ExceptionOr<ByteArray> result,
                                        bool closed) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_read_buffer_.reset();
  pending_read_buffer_pos_ = 0;
  if (!pending_read_waitable_event_) {
    return;
  }
  *pending_read_result_ = std::move(result);
  *pending_read_closed_ = closed;
  pending_read_result_ = nullptr;
  pending_read_closed_ = nullptr;
  // Clear |pending_read_waitable_event_| before signaling so |this| holds no
  // dangling pointer once the waiting Read() thread wakes and unwinds its
  // stack.
  base::WaitableEvent* read_waitable_event = pending_read_waitable_event_;
  pending_read_waitable_event_ = nullptr;
  read_waitable_event->Signal();
}

bool InputStreamImpl::IsClosed() const {
  base::AutoLock lock(lock_);
  return is_closed_;
}

void InputStreamImpl::OnReadFinished() {
  base::WaitableEvent* drain_event = nullptr;
  {
    base::AutoLock lock(lock_);
    CHECK_GT(reads_in_flight_, 0u);
    --reads_in_flight_;
    if (reads_in_flight_ == 0 && drain_waitable_event_) {
      drain_event = drain_waitable_event_;
      drain_waitable_event_ = nullptr;
    }
  }
  // Signal outside |lock_| and without touching |this| again: once signaled,
  // WaitForInFlightReads() may return and destroy |this|.
  if (drain_event) {
    drain_event->Signal();
  }
}

void InputStreamImpl::DoClose(base::WaitableEvent* task_run_waitable_event) {
  // Must cancel |receive_stream_watcher_| on the same sequence it was
  // initialized on.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (receive_stream_.is_valid()) {
    receive_stream_watcher_.Cancel();
    receive_stream_.reset();

    // It is possible that a Read() call could still be blocking a different
    // sequence via |pending_read_waitable_event_| when Close() is called. If we
    // only cancel the stream watcher, the Read() call will block forever. We
    // trigger the event manually here, which will cause an IO exception to be
    // returned from Read().
    FinishPendingRead(ExceptionOr<ByteArray>(Exception::kIo), /*closed=*/true);
  }

  if (task_run_waitable_event) {
    task_run_waitable_event->Signal();
  }
}

}  // namespace nearby::chrome
