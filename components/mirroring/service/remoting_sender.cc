// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/remoting_sender.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/cast_streaming/public/decoder_buffer_reader.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/sender/openscreen_frame_sender.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"
#include "third_party/openscreen/src/cast/streaming/sender.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace mirroring {
namespace {

std::unique_ptr<media::cast::SenderEncodedFrame> CreateEncodedFrame(
    const media::DecoderBuffer& decoder_buffer,
    media::cast::FrameId frame_id) {
  auto remoting_frame = std::make_unique<media::cast::SenderEncodedFrame>();
  remoting_frame->frame_id = frame_id;
  remoting_frame->dependency = decoder_buffer.is_key_frame()
                                   ? Dependency::kKeyFrame
                                   : Dependency::kDependent;
  remoting_frame->referenced_frame_id =
      remoting_frame->dependency == Dependency::kKeyFrame ? frame_id
                                                          : frame_id - 1;
  remoting_frame->data =
      std::string(reinterpret_cast<const char*>(decoder_buffer.data()),
                  decoder_buffer.data_size());

  // TODO(crbug.com/1409620): Use duration for reference_time and
  // encode_completion_time instead of timestamp.
  const auto timestamp = base::TimeTicks() + decoder_buffer.timestamp();
  remoting_frame->reference_time = timestamp;
  remoting_frame->encode_completion_time = timestamp;
  remoting_frame->rtp_timestamp =
      media::cast::RtpTimeTicks() +
      ToRtpTimeDelta(decoder_buffer.timestamp(),
                     media::cast::kRemotingRtpTimebase);

  return remoting_frame;
}

}  // namespace

RemotingSender::RemotingSender(
    scoped_refptr<media::cast::CastEnvironment> cast_environment,
    media::cast::CastTransport* transport,
    const media::cast::FrameSenderConfig& config,
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> stream_sender,
    base::OnceClosure error_callback)
    : RemotingSender(cast_environment,
                     media::cast::FrameSender::Create(cast_environment,
                                                      config,
                                                      transport,
                                                      *this),
                     config,
                     std::move(pipe),
                     std::move(stream_sender),
                     std::move(error_callback)) {}

RemotingSender::RemotingSender(
    scoped_refptr<media::cast::CastEnvironment> cast_environment,
    std::unique_ptr<openscreen::cast::Sender> sender,
    const media::cast::FrameSenderConfig& config,
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> stream_sender,
    base::OnceClosure error_callback)
    : RemotingSender(cast_environment,
                     media::cast::FrameSender::Create(cast_environment,
                                                      config,
                                                      std::move(sender),
                                                      *this),
                     config,
                     std::move(pipe),
                     std::move(stream_sender),
                     std::move(error_callback)) {
  DCHECK(base::FeatureList::IsEnabled(media::kOpenscreenCastStreamingSession));
}

RemotingSender::RemotingSender(
    scoped_refptr<media::cast::CastEnvironment> cast_environment,
    std::unique_ptr<media::cast::FrameSender> frame_sender,
    const media::cast::FrameSenderConfig& config,
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> stream_sender,
    base::OnceClosure error_callback)
    : frame_sender_(std::move(frame_sender)),
      clock_(cast_environment->Clock()),
      error_callback_(std::move(error_callback)),
      decoder_buffer_reader_(
          std::make_unique<cast_streaming::DecoderBufferReader>(
              base::BindRepeating(&RemotingSender::OnFrameRead,
                                  base::Unretained(this)),
              std::move(pipe))),
      stream_sender_(this, std::move(stream_sender)) {
  stream_sender_.set_disconnect_handler(base::BindOnce(
      &RemotingSender::OnRemotingDataStreamError, base::Unretained(this)));

  // Eventually calls OnBufferRead().
  decoder_buffer_reader_->ReadBufferAsync();
}

RemotingSender::~RemotingSender() {}

void RemotingSender::SendFrame(media::mojom::DecoderBufferPtr buffer,
                               SendFrameCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decoder_buffer_reader_);

  if (read_complete_cb_ || next_frame_) {
    // This should never occur if the API is being used as intended, as only
    // one SendFrame() call should be ongoing at a time.
    mojo::ReportBadMessage(
        "Multiple calls made to RemotingDataStreamSender::SendFrame()");
    return;
  }

  read_complete_cb_ = std::move(callback);
  decoder_buffer_reader_->ProvideBuffer(std::move(buffer));
}

void RemotingSender::CancelInFlightData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  flow_restart_pending_ = true;
  if (next_frame_) {
    DCHECK(read_complete_cb_);
    next_frame_.reset();
    std::move(read_complete_cb_).Run();
  }
}

int RemotingSender::GetNumberOfFramesInEncoder() const {
  NOTREACHED();
  return 0;
}

base::TimeDelta RemotingSender::GetEncoderBacklogDuration() const {
  NOTREACHED();
  return base::TimeDelta();
}

void RemotingSender::OnFrameCanceled(media::cast::FrameId frame_id) {
  if (next_frame_) {
    TrySendFrame();
  }
}

void RemotingSender::TrySendFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(next_frame_);
  DCHECK(read_complete_cb_);

  // If there would be too many frames in-flight, do not proceed.
  if (frame_sender_->GetUnacknowledgedFrameCount() >=
      media::cast::kMaxUnackedFrames) {
    VLOG(1) << "Cannot send frame now because too many frames are in flight.";
    return;
  }

  DCHECK(flow_restart_pending_ ||
         (next_frame_id_ != media::cast::FrameId::first()));

  auto remoting_frame = CreateEncodedFrame(*next_frame_, next_frame_id_);
  if (flow_restart_pending_) {
    remoting_frame->dependency = Dependency::kKeyFrame;
    remoting_frame->referenced_frame_id = remoting_frame->frame_id;
    flow_restart_pending_ = false;
  }

  const auto rtp_timestamp = remoting_frame->rtp_timestamp;
  if (frame_sender_->EnqueueFrame(std::move(remoting_frame))) {
    // Only increment if we successfully enqueued.
    next_frame_id_++;
  } else {
    TRACE_EVENT_INSTANT2("cast.stream", "Remoting Frame Drop",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         rtp_timestamp.lower_32_bits(), "reason",
                         "openscreen sender did not accept the frame");
  }

  next_frame_.reset();
  decoder_buffer_reader_->ReadBufferAsync();
  std::move(read_complete_cb_).Run();
}

void RemotingSender::OnFrameRead(scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!next_frame_);
  DCHECK(read_complete_cb_);
  DCHECK(decoder_buffer_reader_);

  next_frame_ = std::move(buffer);

  TrySendFrame();
}

void RemotingSender::OnRemotingDataStreamError() {
  // NOTE: This method must be idemptotent as it may be called more than once.
  decoder_buffer_reader_.reset();
  stream_sender_.reset();
  if (!error_callback_.is_null())
    std::move(error_callback_).Run();
}

bool RemotingSender::HadError() const {
  DCHECK_EQ(!decoder_buffer_reader_, !stream_sender_.is_bound());
  return !decoder_buffer_reader_;
}

}  // namespace mirroring
