// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/frame/stream_consumer.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/browser/common/decoder_buffer_factory.h"
#include "components/cast_streaming/common/public/features.h"
#include "media/base/media_util.h"
#include "media/mojo/common/media_type_converters.h"
#include "third_party/openscreen/src/platform/base/span.h"

namespace cast_streaming {

StreamConsumer::StreamConsumer(
    openscreen::cast::Receiver* receiver,
    mojo::ScopedDataPipeProducerHandle data_pipe,
    FrameReceivedCB frame_received_cb,
    base::RepeatingClosure on_new_frame,
    std::unique_ptr<DecoderBufferFactory> decoder_buffer_factory)
    : receiver_(receiver),
      data_pipe_(std::move(data_pipe)),
      frame_received_cb_(std::move(frame_received_cb)),
      pipe_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunner::GetCurrentDefault()),
      on_new_frame_(std::move(on_new_frame)),
      decoder_buffer_factory_(std::move(decoder_buffer_factory)) {
  DCHECK(receiver_);
  DCHECK(decoder_buffer_factory_);

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

// NOTE: Do NOT call into `receiver_` methods here, as the object may no longer
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

  DCHECK(pending_buffer_);
  size_t bytes_written = 0;
  base::span<const uint8_t> remaining_data =
      base::span<const uint8_t>(*pending_buffer_)
          .subspan(pending_buffer_offset_);

  result = data_pipe_->WriteData(remaining_data, MOJO_WRITE_DATA_FLAG_NONE,
                                 bytes_written);
  if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }

  pending_buffer_offset_ += base::checked_cast<size_t>(bytes_written);
  if (pending_buffer_offset_ < pending_buffer_->size()) {
    pipe_watcher_.ArmOrNotify();
    return;
  }

  pending_buffer_.reset();
  pending_buffer_offset_ = 0;
  MaybeSendNextFrame();
}

void StreamConsumer::OnFramesReady(int next_frame_buffer_size) {
  MaybeSendNextFrame();
}

void StreamConsumer::FlushUntil(uint32_t frame_id) {
  skip_until_frame_id_ = frame_id;
  if (is_read_pending_) {
    is_read_pending_ = false;
    no_frames_available_cb_.Reset();
    frame_received_cb_.Run(nullptr);
  }
}

StreamConsumer::FrameBuffer::FrameBuffer() {
  pending_buffer_.reserve(kInitialFrameSize);
}

StreamConsumer::FrameBuffer::FrameBuffer(StreamConsumer::FrameBuffer&&) =
    default;
StreamConsumer::FrameBuffer& StreamConsumer::FrameBuffer::operator=(
    StreamConsumer::FrameBuffer&&) = default;

StreamConsumer::FrameBuffer::~FrameBuffer() = default;

base::span<uint8_t> StreamConsumer::FrameBuffer::as_span() {
  return base::span<uint8_t>(pending_buffer_);
}

bool StreamConsumer::FrameBuffer::Resize(size_t new_size) {
  if (new_size > kMaxFrameSize) {
    return false;
  }

  // If we need to grow, reserve extra capacity to avoid frequent reallocations
  // for slightly larger frames (e.g. X followed by X+1).
  if (pending_buffer_.capacity() < new_size) {
    // Grow by 50% extra up to the max frame size.
    const size_t to_reserve =
        std::min(kMaxFrameSize, static_cast<size_t>(new_size + (new_size / 2)));
    pending_buffer_.reserve(to_reserve);
  }

  // Ensure the buffer size exactly matches the extent of the incoming frame.
  pending_buffer_.resize(new_size);

  return true;
}

void StreamConsumer::MaybeSendNextFrame() {
  if (!is_read_pending_ || pending_buffer_) {
    return;
  }

  const int current_frame_buffer_size = receiver_->AdvanceToNextFrame();
  if (current_frame_buffer_size == openscreen::cast::Receiver::kNoFramesReady) {
    if (no_frames_available_cb_) {
      std::move(no_frames_available_cb_).Run();
    }
    return;
  }

  on_new_frame_.Run();

  if (!frame_buffer_.Resize(current_frame_buffer_size)) {
    LOG(ERROR) << "[ssrc:" << receiver_->ssrc() << "] "
               << "Frame size too big: " << current_frame_buffer_size
               << " (implies corrupt or malicious stream)";
    CloseDataPipeOnError();
    return;
  }

  openscreen::cast::EncodedFrame encoded_frame;

  // Write to temporary storage in case we need to drop this frame.
  encoded_frame = receiver_->ConsumeNextFrame(frame_buffer_.as_span());

  // If the frame occurs before the id we want to flush until, drop it and try
  // again.
  // TODO(crbug.com/1412561): Move this logic to Openscreen.
  if (encoded_frame.frame_id <
      openscreen::cast::FrameId(int64_t{skip_until_frame_id_})) {
    VLOG(1) << "Skipping Frame " << encoded_frame.frame_id;

    MaybeSendNextFrame();
    return;
  }

  // Create the buffer, retrying if this fails.
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      decoder_buffer_factory_->ToDecoderBuffer(encoded_frame,
                                               frame_buffer_.as_span());
  if (!decoder_buffer) {
    MaybeSendNextFrame();
    return;
  }

  // At this point, the frame is known to be "good".
  skip_until_frame_id_ = 0;
  no_frames_available_cb_.Reset();

  pending_buffer_ = std::move(decoder_buffer);
  pending_buffer_offset_ = 0;

  // Write the frame's data to Mojo.
  size_t bytes_written = 0;
  auto result = data_pipe_->WriteData(*pending_buffer_,
                                      MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    pipe_watcher_.ArmOrNotify();
    bytes_written = 0;
  } else if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }
  pending_buffer_offset_ += base::checked_cast<size_t>(bytes_written);

  // Return the frame.
  is_read_pending_ = false;
  frame_received_cb_.Run(media::mojom::DecoderBuffer::From(*pending_buffer_));

  // Wait for the mojo pipe to be writable if there is still pending data to
  // write.
  if (pending_buffer_offset_ < pending_buffer_->size()) {
    pipe_watcher_.ArmOrNotify();
  } else {
    pending_buffer_.reset();
    pending_buffer_offset_ = 0;
  }
}

}  // namespace cast_streaming
