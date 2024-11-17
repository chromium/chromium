// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/output_stream_impl.h"

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
      break;
  }
}

}  // namespace

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
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Close();
}

Exception OutputStreamImpl::Write(const ByteArray& data) {
  if (IsClosed()) {
    return {Exception::kIo};
  }

  DCHECK(!write_success_);
  pending_write_buffer_ = std::make_unique<ByteArray>(data);
  pending_write_buffer_pos_ = 0;

  // Signal and reset the WaitableEvent in case another thread is already
  // waiting on a Write().
  write_waitable_event_.Signal();
  write_waitable_event_.Reset();

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&mojo::SimpleWatcher::ArmOrNotify,
                                base::Unretained(&send_stream_watcher_)));
  write_waitable_event_.Wait();

  Exception result = {write_success_ ? Exception::kSuccess : Exception::kIo};

  write_success_ = false;
  pending_write_buffer_.reset();
  pending_write_buffer_pos_ = 0;

  // |send_stream_| might have been reset in Close() while
  // |write_waitable_event_| was waiting.
  if (IsClosed()) {
    return {Exception::kIo};
  }

  // Ignore a null |send_stream_| when logging since it might be an expected
  // state as mentioned above.
  LogWriteResult(medium_, result.Ok());

  return result;
}

Exception OutputStreamImpl::Flush() {
  // TODO(hansberry): Unsure if anything can reasonably be done here. Need to
  // ask a reviewer from the Nearby team.
  return {Exception::kSuccess};
}

Exception OutputStreamImpl::Close() {
  // NOTE(http://crbug.com/1247876): Close() might be called from multiple
  // threads at the same time; sequence calls and check if stream is already
  // closed inside task. Also, must cancel |send_stream_watcher_| on the same
  // sequence it was initialized on.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    // No need to post a task if this is already running on |task_runner_|.
    DoClose(/*task_run_waitable_event=*/nullptr);
  } else {
    base::WaitableEvent task_run_waitable_event;
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&OutputStreamImpl::DoClose,
                                                     base::Unretained(this),
                                                     &task_run_waitable_event));
    task_run_waitable_event.Wait();
  }

  return {Exception::kSuccess};
}

void OutputStreamImpl::SendMore(MojoResult result,
                                const mojo::HandleSignalsState& state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(result, MOJO_RESULT_SHOULD_WAIT);
  DCHECK(!IsClosed());
  DCHECK(pending_write_buffer_);
  DCHECK_LT(pending_write_buffer_pos_, pending_write_buffer_->size());

  if (state.peer_closed()) {
    write_success_ = false;
    write_waitable_event_.Signal();
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

  write_success_ = result == MOJO_RESULT_OK;
  write_waitable_event_.Signal();
}

bool OutputStreamImpl::IsClosed() const {
  return !send_stream_.is_valid();
}

void OutputStreamImpl::DoClose(base::WaitableEvent* task_run_waitable_event) {
  // Must cancel |send_stream_watcher_| on the same sequence it was
  // initialized on.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!IsClosed()) {
    send_stream_watcher_.Cancel();
    send_stream_.reset();

    // It is possible that a Write() call could still be blocking a different
    // sequence via |write_waitable_event_| when Close() is called. If we only
    // cancel the stream watcher, the Write() call will block forever. We
    // trigger the event manually here, which will cause an IO exception to be
    // returned from Write().
    write_waitable_event_.Signal();
  }

  if (task_run_waitable_event) {
    task_run_waitable_event->Signal();
  }
}

}  // namespace nearby::chrome
