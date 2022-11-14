// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/stream_consumer.h"

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_util.h"

namespace cast_streaming {

StreamConsumer::StreamConsumer(openscreen::cast::Receiver* receiver,
                               base::TimeDelta frame_duration,
                               mojo::ScopedDataPipeProducerHandle data_pipe,
                               FrameReceivedCB frame_received_cb,
                               base::RepeatingClosure on_new_frame)
    : receiver_(receiver),
      data_pipe_(std::move(data_pipe)),
      frame_received_cb_(std::move(frame_received_cb)),
      pipe_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunner::GetCurrentDefault()),
      frame_duration_(frame_duration),
      on_new_frame_(std::move(on_new_frame)) {
  DCHECK(receiver_);
  receiver_->SetConsumer(this);
  MojoResult result =
      pipe_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                          base::BindRepeating(&StreamConsumer::OnPipeWritable,
                                              base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }
}

StreamConsumer::StreamConsumer(StreamConsumer&& other,
                               openscreen::cast::Receiver* receiver,
                               mojo::ScopedDataPipeProducerHandle data_pipe)
    : StreamConsumer(receiver,
                     other.frame_duration_,
                     std::move(data_pipe),
                     std::move(other.frame_received_cb_),
                     std::move(other.on_new_frame_)) {
  if (other.is_read_pending_) {
    ReadFrame(std::move(other.no_frames_available_cb_));
  }
}

// NOTE: Do NOT call into |receiver_| methods here, as the object may no longer
// be valid at time of this object's destruction.
StreamConsumer::~StreamConsumer() = default;

void StreamConsumer::ReadFrame(base::OnceClosure no_frames_available_cb) {
  DCHECK(!is_read_pending_);
  DCHECK(!no_frames_available_cb_);
  is_read_pending_ = true;
  no_frames_available_cb_ = std::move(no_frames_available_cb);
  MaybeSendNextFrame();
}

void StreamConsumer::CloseDataPipeOnError() {
  DLOG(WARNING) << "[ssrc:" << receiver_->ssrc() << "] Data pipe closed.";
  pipe_watcher_.Cancel();
  data_pipe_.reset();
}

void StreamConsumer::OnPipeWritable(MojoResult result) {
  DCHECK(data_pipe_);

  if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }

  uint32_t bytes_written = pending_buffer_remaining_bytes_;
  result = data_pipe_->WriteData(pending_buffer_ + pending_buffer_offset_,
                                 &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }

  pending_buffer_offset_ += bytes_written;
  pending_buffer_remaining_bytes_ -= bytes_written;
  if (pending_buffer_remaining_bytes_ != 0) {
    pipe_watcher_.ArmOrNotify();
    return;
  }

  MaybeSendNextFrame();
}

void StreamConsumer::OnFramesReady(int next_frame_buffer_size) {
  MaybeSendNextFrame();
}

void StreamConsumer::MaybeSendNextFrame() {
  if (!is_read_pending_ || pending_buffer_remaining_bytes_ > 0) {
    return;
  }

  const int current_frame_buffer_size = receiver_->AdvanceToNextFrame();
  if (current_frame_buffer_size == openscreen::cast::Receiver::kNoFramesReady) {
    if (no_frames_available_cb_) {
      std::move(no_frames_available_cb_).Run();
    }
    return;
  }

  no_frames_available_cb_.Reset();
  on_new_frame_.Run();

  void* buffer = nullptr;
  uint32_t buffer_size = current_frame_buffer_size;
  uint32_t mojo_buffer_size = current_frame_buffer_size;

  if (buffer_size > kMaxFrameSize) {
    LOG(ERROR) << "[ssrc:" << receiver_->ssrc() << "] "
               << "Frame size too big: " << buffer_size;
    CloseDataPipeOnError();
    return;
  }

  MojoResult result = data_pipe_->BeginWriteData(
      &buffer, &mojo_buffer_size, MOJO_BEGIN_WRITE_DATA_FLAG_NONE);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    pipe_watcher_.ArmOrNotify();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }

  openscreen::cast::EncodedFrame encoded_frame;
  size_t bytes_written = 0;

  if (mojo_buffer_size < buffer_size) {
    DVLOG(2) << "[ssrc:" << receiver_->ssrc() << "] "
             << "Mojo data pipe full";

    // The |data_pipe_| buffer cannot take the full frame, write to
    // |pending_buffer_| instead.
    encoded_frame = receiver_->ConsumeNextFrame(
        absl::Span<uint8_t>(pending_buffer_, buffer_size));

    // Write as much as we can to the |data_pipe_| buffer.
    memcpy(buffer, pending_buffer_, mojo_buffer_size);
    pending_buffer_offset_ = mojo_buffer_size;
    pending_buffer_remaining_bytes_ = buffer_size - mojo_buffer_size;
    bytes_written = mojo_buffer_size;
  } else {
    // Write directly to the |data_pipe_| buffer.
    encoded_frame = receiver_->ConsumeNextFrame(
        absl::Span<uint8_t>(static_cast<uint8_t*>(buffer), buffer_size));
    bytes_written = buffer_size;
  }

  result = data_pipe_->EndWriteData(bytes_written);
  if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }

  const bool is_key_frame =
      encoded_frame.dependency ==
      openscreen::cast::EncodedFrame::Dependency::kKeyFrame;

  base::TimeDelta playout_time =
      base::Microseconds(std::chrono::duration_cast<std::chrono::microseconds>(
                             encoded_frame.reference_time.time_since_epoch())
                             .count());

  // Some senders do not send an initial playout time of 0. To work around this,
  // a playout offset is added here.
  if (playout_offset_ == base::TimeDelta::Max()) {
    playout_offset_ = playout_time;
  }
  playout_time -= playout_offset_;

  DVLOG(3) << "[ssrc:" << receiver_->ssrc() << "] "
           << "Received new frame. Timestamp: " << playout_time
           << ", is_key_frame: " << is_key_frame;

  is_read_pending_ = false;
  frame_received_cb_.Run(media::mojom::DecoderBuffer::New(
      playout_time /* timestamp */, frame_duration_,
      false /* is_end_of_stream */, buffer_size, is_key_frame,
      media::EmptyExtraData(), media::mojom::DecryptConfigPtr(),
      base::TimeDelta() /* front_discard */,
      base::TimeDelta() /* back_discard */
      ));

  if (pending_buffer_remaining_bytes_ != 0) {
    pipe_watcher_.ArmOrNotify();
  }
}

}  // namespace cast_streaming
