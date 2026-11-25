// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/output_stream_impl.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/stack_allocated.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"

namespace nearby::chrome {

namespace {

void LogWriteResult(connections::mojom::Medium medium, bool success) {
  switch (medium) {
    case connections::mojom::Medium::kBluetooth:
      base::UmaHistogramBoolean(
          "Nearby.Connections.Bluetooth.Socket.Write.Result", success);
      break;
    case connections::mojom::Medium::kWifiLan:
      base::UmaHistogramBoolean(
          "Nearby.Connections.WifiLan.Socket.Write.Result", success);
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

class OutputStreamImpl::ScopedInFlightWrite {
  STACK_ALLOCATED();

 public:
  explicit ScopedInFlightWrite(OutputStreamImpl* stream) : stream_(stream) {}
  ~ScopedInFlightWrite() { stream_->OnWriteFinished(); }
  ScopedInFlightWrite(const ScopedInFlightWrite&) = delete;
  ScopedInFlightWrite& operator=(const ScopedInFlightWrite&) = delete;

 private:
  OutputStreamImpl* const stream_;
};

OutputStreamImpl::OutputStreamImpl(
    connections::mojom::Medium medium,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::ScopedDataPipeProducerHandle send_stream)
    : medium_(medium),
      task_runner_(std::move(task_runner)),
      send_stream_(std::move(send_stream)),
      send_stream_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                           task_runner_) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  send_stream_watcher_.Watch(
      send_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&OutputStreamImpl::SendMore, base::Unretained(this)));
}

OutputStreamImpl::~OutputStreamImpl() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  Close();
  base::AutoLock lock(lock_);
  CHECK_EQ(writes_in_flight_, 0u);
}

Exception OutputStreamImpl::Write(const ByteArray& data) {
  CHECK(!task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    if (is_closed_) {
      return {Exception::kIo};
    }
    ++writes_in_flight_;
  }
  ScopedInFlightWrite scoped_in_flight(this);

  base::WaitableEvent write_waitable_event;
  bool write_success = false;
  bool closed_during_write = false;

  if (!task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&OutputStreamImpl::StartWrite, base::Unretained(this),
                         std::make_unique<ByteArray>(data),
                         &write_waitable_event, &write_success,
                         &closed_during_write))) {
    return {Exception::kIo};
  }
  write_waitable_event.Wait();

  // Skip LogWriteResult() when Close() interrupts a pending Write(), since
  // that is expected during normal teardown.
  if (closed_during_write) {
    return {Exception::kIo};
  }

  Exception result = {write_success ? Exception::kSuccess : Exception::kIo};
  LogWriteResult(medium_, result.Ok());

  return result;
}

Exception OutputStreamImpl::Flush() {
  // TODO(hansberry): Unsure if anything can reasonably be done here. Need to
  // ask a reviewer from the Nearby team.
  return {Exception::kSuccess};
}

Exception OutputStreamImpl::Close() {
  {
    base::AutoLock lock(lock_);
    is_closed_ = true;
  }

  // NOTE(http://crbug.com/40197037): Close() might be called from multiple
  // threads at the same time; sequence calls and check if stream is already
  // closed inside task. Also, must cancel |send_stream_watcher_| on the same
  // sequence it was initialized on.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    // No need to post a task if this is already running on |task_runner_|.
    DoClose(/*task_run_waitable_event=*/nullptr);
  } else {
    base::WaitableEvent task_run_waitable_event;
    if (task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&OutputStreamImpl::DoClose, base::Unretained(this),
                           &task_run_waitable_event))) {
      task_run_waitable_event.Wait();
    }
  }

  return {Exception::kSuccess};
}

void OutputStreamImpl::WaitForInFlightWrites() {
  CHECK(!task_runner_->RunsTasksInCurrentSequence());
  base::WaitableEvent drain_event;
  {
    base::AutoLock lock(lock_);
    CHECK(is_closed_);
    CHECK(!drain_waitable_event_);
    if (writes_in_flight_ == 0) {
      return;
    }
    drain_waitable_event_ = &drain_event;
  }
  drain_event.Wait();
}

void OutputStreamImpl::SetOnWritePendingForTesting(base::OnceClosure callback) {
  base::AutoLock lock(lock_);
  on_write_pending_for_testing_ = std::move(callback);
}

void OutputStreamImpl::StartWrite(std::unique_ptr<ByteArray> data,
                                  base::WaitableEvent* write_waitable_event,
                                  bool* write_success,
                                  bool* closed_during_write) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!send_stream_.is_valid() || IsClosed()) {
    *write_success = false;
    *closed_during_write = true;
    write_waitable_event->Signal();
    return;
  }

  // Wake any previously waiting Write() before replacing the active write
  // state.
  if (pending_write_waitable_event_) {
    FinishPendingWrite(/*success=*/false, /*closed=*/false);
  }

  pending_write_buffer_ = std::move(data);
  pending_write_buffer_pos_ = 0;
  pending_write_waitable_event_ = write_waitable_event;
  pending_write_success_ = write_success;
  pending_write_closed_ = closed_during_write;

  if (pending_write_buffer_->Empty()) {
    FinishPendingWrite(/*success=*/true, /*closed=*/false);
    return;
  }

  send_stream_watcher_.ArmOrNotify();

  base::OnceClosure on_write_pending;
  {
    base::AutoLock lock(lock_);
    on_write_pending = std::move(on_write_pending_for_testing_);
  }
  if (on_write_pending) {
    std::move(on_write_pending).Run();
  }
}

void OutputStreamImpl::SendMore(MojoResult result,
                                const mojo::HandleSignalsState& state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(result, MOJO_RESULT_SHOULD_WAIT);
  DCHECK(send_stream_.is_valid());
  if (!pending_write_waitable_event_) {
    return;
  }
  DCHECK(pending_write_buffer_);
  DCHECK_LT(pending_write_buffer_pos_, pending_write_buffer_->size());

  if (state.peer_closed()) {
    FinishPendingWrite(/*success=*/false, /*closed=*/false);
    return;
  }

  if (result == MOJO_RESULT_OK) {
    base::span<const uint8_t> buffer =
        base::as_byte_span(*pending_write_buffer_)
            .subspan(pending_write_buffer_pos_);
    size_t bytes_written = 0;
    result = send_stream_->WriteData(buffer, MOJO_WRITE_DATA_FLAG_NONE,
                                     bytes_written);
    if (result == MOJO_RESULT_OK) {
      pending_write_buffer_pos_ += bytes_written;
    }
  }

  if (result == MOJO_RESULT_SHOULD_WAIT ||
      pending_write_buffer_pos_ < pending_write_buffer_->size()) {
    send_stream_watcher_.ArmOrNotify();
    return;
  }

  FinishPendingWrite(/*success=*/result == MOJO_RESULT_OK, /*closed=*/false);
}

void OutputStreamImpl::FinishPendingWrite(bool success, bool closed) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_write_buffer_.reset();
  pending_write_buffer_pos_ = 0;
  if (!pending_write_waitable_event_) {
    return;
  }
  *pending_write_success_ = success;
  *pending_write_closed_ = closed;
  pending_write_success_ = nullptr;
  pending_write_closed_ = nullptr;
  // Clear |pending_write_waitable_event_| before signaling so |this| holds no
  // dangling pointer once the waiting Write() thread wakes and unwinds its
  // stack.
  base::WaitableEvent* write_waitable_event = pending_write_waitable_event_;
  pending_write_waitable_event_ = nullptr;
  write_waitable_event->Signal();
}

bool OutputStreamImpl::IsClosed() const {
  base::AutoLock lock(lock_);
  return is_closed_;
}

void OutputStreamImpl::OnWriteFinished() {
  base::WaitableEvent* drain_event = nullptr;
  {
    base::AutoLock lock(lock_);
    CHECK_GT(writes_in_flight_, 0u);
    --writes_in_flight_;
    if (writes_in_flight_ == 0 && drain_waitable_event_) {
      drain_event = drain_waitable_event_;
      drain_waitable_event_ = nullptr;
    }
  }
  // Signal outside |lock_| and without touching |this| again: once signaled,
  // WaitForInFlightWrites() may return and destroy |this|.
  if (drain_event) {
    drain_event->Signal();
  }
}

void OutputStreamImpl::DoClose(base::WaitableEvent* task_run_waitable_event) {
  // Must cancel |send_stream_watcher_| on the same sequence it was
  // initialized on.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (send_stream_.is_valid()) {
    send_stream_watcher_.Cancel();
    send_stream_.reset();

    // It is possible that a Write() call could still be blocking a different
    // sequence via |pending_write_waitable_event_| when Close() is called. If
    // we only cancel the stream watcher, the Write() call will block forever.
    // We trigger the event manually here, which will cause an IO exception to
    // be returned from Write().
    FinishPendingWrite(/*success=*/false, /*closed=*/true);
  }

  if (task_run_waitable_event) {
    task_run_waitable_event->Signal();
  }
}

}  // namespace nearby::chrome
