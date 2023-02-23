// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/frame/stream_consumer.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/public/features.h"
#include "components/cast_streaming/public/remoting_proto_utils.h"
#include "media/base/media_util.h"
#include "media/mojo/common/media_type_converters.h"
#include "third_party/openscreen/src/platform/base/span.h"

namespace cast_streaming {

base::span<uint8_t> StreamConsumer::BufferDataWrapper::Get() {
  return base::span<uint8_t>(&pending_buffer_[pending_buffer_offset_],
                             pending_buffer_remaining_bytes_);
}

base::span<uint8_t> StreamConsumer::BufferDataWrapper::Consume(
    uint32_t max_size) {
  const uint32_t current_offset = pending_buffer_offset_;
  const uint32_t current_remaining_bytes = pending_buffer_remaining_bytes_;

  const uint32_t read_size = std::min(max_size, current_remaining_bytes);

  pending_buffer_offset_ += read_size;
  pending_buffer_remaining_bytes_ -= read_size;
  return base::span<uint8_t>(&pending_buffer_[current_offset], read_size);
}

bool StreamConsumer::BufferDataWrapper::Reset(uint32_t new_size) {
  if (new_size > kMaxFrameSize) {
    return false;
  }

  pending_buffer_offset_ = 0;
  pending_buffer_remaining_bytes_ = new_size;
  return true;
}

void StreamConsumer::BufferDataWrapper::Clear() {
  bool success = Reset(uint32_t{0});
  DCHECK(success);
}

StreamConsumer::StreamConsumer(openscreen::cast::Receiver* receiver,
                               base::TimeDelta frame_duration,
                               mojo::ScopedDataPipeProducerHandle data_pipe,
                               FrameReceivedCB frame_received_cb,
                               base::RepeatingClosure on_new_frame,
                               bool is_remoting)
    : receiver_(receiver),
      data_pipe_(std::move(data_pipe)),
      frame_received_cb_(std::move(frame_received_cb)),
      pipe_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunner::GetCurrentDefault()),
      frame_duration_(frame_duration),
      is_remoting_(is_remoting),
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
                     std::move(other.on_new_frame_),
                     other.is_remoting_) {
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

  base::span<uint8_t> span = data_wrapper_.Get();
  uint32_t bytes_written = span.size();
  result = data_pipe_->WriteData(span.data(), &bytes_written,
                                 MOJO_WRITE_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }

  data_wrapper_.Consume(bytes_written);
  if (!data_wrapper_.empty()) {
    pipe_watcher_.ArmOrNotify();
    return;
  }

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

void StreamConsumer::MaybeSendNextFrame() {
  if (!is_read_pending_ || !data_wrapper_.empty()) {
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

  if (!data_wrapper_.Reset(current_frame_buffer_size)) {
    LOG(ERROR) << "[ssrc:" << receiver_->ssrc() << "] "
               << "Frame size too big: " << current_frame_buffer_size;
    CloseDataPipeOnError();
    return;
  }

  openscreen::cast::EncodedFrame encoded_frame;

  // Write to temporary storage in case we need to drop this frame.
  base::span<uint8_t> span = data_wrapper_.Get();
  encoded_frame = receiver_->ConsumeNextFrame(
      openscreen::ByteBuffer(span.data(), span.size()));

  // If the frame occurs before the id we want to flush until, drop it and try
  // again.
  // TODO(crbug.com/1412561): Move this logic to Openscreen.
  if (encoded_frame.frame_id <
      openscreen::cast::FrameId(int64_t{skip_until_frame_id_})) {
    VLOG(1) << "Skipping Frame " << encoded_frame.frame_id;

    data_wrapper_.Clear();
    MaybeSendNextFrame();
    return;
  }

  // Create the buffer, retrying if this fails.
  //
  // NOTE: Using CreateRemotingBuffer() is EXPECTED for all remoting streams,
  // but REQUIRED only for certain codecs - so inconsistent behavior rather than
  // just "not working" will be observed if the wrong call is made.
  scoped_refptr<media::DecoderBuffer> decoder_buffer;
  if (is_remoting_) {
    decoder_buffer = CreateRemotingBuffer();
  } else {
    decoder_buffer = CreateMirroringBuffer(encoded_frame);
  }

  if (!decoder_buffer) {
    data_wrapper_.Clear();
    MaybeSendNextFrame();
  }

  // At this point, the frame is known to be "good".
  skip_until_frame_id_ = 0;
  no_frames_available_cb_.Reset();

  // Write the frame's data to Mojo.
  span = data_wrapper_.Get();
  uint32_t bytes_written = span.size();
  auto result = data_pipe_->WriteData(span.data(), &bytes_written,
                                      MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    pipe_watcher_.ArmOrNotify();
    bytes_written = 0;
  } else if (result != MOJO_RESULT_OK) {
    CloseDataPipeOnError();
    return;
  }
  data_wrapper_.Consume(bytes_written);

  // Return the frame.
  is_read_pending_ = false;
  frame_received_cb_.Run(media::mojom::DecoderBuffer::From(*decoder_buffer));

  // Wait for the mojo pipe to be writable if there is still pending data to
  // write.
  if (!data_wrapper_.empty()) {
    pipe_watcher_.ArmOrNotify();
  }
}

scoped_refptr<media::DecoderBuffer> StreamConsumer::CreateRemotingBuffer() {
  DCHECK(is_remoting_);

  auto span = data_wrapper_.Get();
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      remoting::ByteArrayToDecoderBuffer(span.data(), span.size());
  if (!decoder_buffer) {
    DLOG(WARNING) << "Deserialization failed!";
    return nullptr;
  }

  if (!data_wrapper_.Reset(decoder_buffer->data_size())) {
    DLOG(WARNING) << "Buffer overflow!";
    return nullptr;
  }

  span = data_wrapper_.Get();
  base::span<const uint8_t> decoder_buffer_data(decoder_buffer->data(),
                                                decoder_buffer->data_size());
  std::copy(decoder_buffer_data.begin(), decoder_buffer_data.end(),
            span.begin());

  return decoder_buffer;
}

scoped_refptr<media::DecoderBuffer> StreamConsumer::CreateMirroringBuffer(
    const openscreen::cast::EncodedFrame& encoded_frame) {
  DCHECK(!is_remoting_);

  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      base::MakeRefCounted<media::DecoderBuffer>(data_wrapper_.size());

  decoder_buffer->set_duration(frame_duration_);
  decoder_buffer->set_is_key_frame(
      encoded_frame.dependency ==
      openscreen::cast::EncodedFrame::Dependency::kKeyFrame);

  base::TimeDelta playout_time =
      base::Microseconds(encoded_frame.rtp_timestamp
                             .ToTimeSinceOrigin<std::chrono::microseconds>(
                                 receiver_->rtp_timebase())
                             .count());

  // Some senders do not send an initial playout time of 0. To work around this,
  // a playout offset is added here. This is NOT done when remoting is enabled
  // because the timestamp of the first frame is used to automatically start
  // playback in such cases.
  if (!IsCastRemotingEnabled()) {
    if (playout_offset_ == base::TimeDelta::Max()) {
      playout_offset_ = playout_time;
    }
    playout_time -= playout_offset_;
  }

  decoder_buffer->set_timestamp(playout_time);

  DVLOG(3) << "[ssrc:" << receiver_->ssrc() << "] "
           << "Received new frame. Timestamp: " << playout_time
           << ", is_key_frame: " << decoder_buffer->is_key_frame();

  return decoder_buffer;
}

}  // namespace cast_streaming
