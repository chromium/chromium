// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/remoting_sender.h"

#include <algorithm>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/openscreen/decoder_buffer_reader.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "media/cast/sender/openscreen_frame_sender.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace mirroring {
namespace {

// UMA histograms for recording the percentage of dropped frames.
constexpr char kHistogramDroppedAudioFrames[] =
    "CastStreaming.Sender.Remoting.Audio.PercentDroppedFrames";
constexpr char kHistogramDroppedVideoFrames[] =
    "CastStreaming.Sender.Remoting.Video.PercentDroppedFrames";

// UMA histograms for recording when a frame is dropped.
constexpr char kHistogramAudioFrameDropped[] =
    "CastStreaming.Sender.Remoting.Audio.FrameDropped";
constexpr char kHistogramVideoFrameDropped[] =
    "CastStreaming.Sender.Remoting.Video.FrameDropped";

// Maximum number of consecutive EnqueueFrame() calls that may be made before
// frames begin being dropped.
constexpr int kMaxEnqueueFrameFailures = 3;

}  // namespace

class RemotingSender::SenderEncodedFrameFactory {
 public:
  SenderEncodedFrameFactory(int rtp_timebase,
                            media::cast::FrameSender& frame_sender,
                            const base::TickClock& clock)
      : rtp_timebase_(rtp_timebase),
        frame_sender_(frame_sender),
        clock_(clock) {
    // TODO(crbug.com/1413561): validate that we can use an arbitrary timebase
    // here. Some receivers may require us to use the hardcoded remoting RTP
    // timebase.
    DCHECK_EQ(rtp_timebase_, media::cast::kRemotingRtpTimebase);
  }

  std::unique_ptr<media::cast::SenderEncodedFrame> CreateEncodedFrame(
      const media::DecoderBuffer& decoder_buffer,
      media::cast::FrameId frame_id) {
    frames_created_++;

    auto remoting_frame = std::make_unique<media::cast::SenderEncodedFrame>();
    remoting_frame->frame_id = frame_id;

    // DecoderBuffer data must be encoded in a special format.
    std::vector<uint8_t> data =
        media::cast::DecoderBufferToByteArray(decoder_buffer);
    if (!decoder_buffer.end_of_stream() && data.empty()) {
      return nullptr;
    }
    remoting_frame->data =
        std::string(reinterpret_cast<const char*>(data.data()), data.size());

    const bool is_key_frame =
        !decoder_buffer.end_of_stream() && decoder_buffer.is_key_frame();
    remoting_frame->dependency =
        is_key_frame ? Dependency::kKeyFrame : Dependency::kDependent;
    remoting_frame->referenced_frame_id =
        is_key_frame ? frame_id : frame_id - 1;
    remoting_frame->reference_time = clock_->NowTicks();
    remoting_frame->encode_completion_time = remoting_frame->reference_time;

    base::TimeTicks last_frame_reference_time;
    media::cast::RtpTimeTicks last_frame_rtp_timestamp;
    const bool is_first_frame = (frame_id == media::cast::FrameId::first());
    if (is_first_frame) {
      last_frame_reference_time = remoting_frame->reference_time;
      last_frame_rtp_timestamp =
          media::cast::RtpTimeTicks() - media::cast::RtpTimeDelta::FromTicks(1);
    } else {
      last_frame_reference_time = frame_sender_->LastSendTime();
      last_frame_rtp_timestamp =
          frame_sender_->GetRecordedRtpTimestamp(frame_id - 1);
    }

    // Ensure each successive frame's RTP timestamp is unique, but otherwise
    // just base it on the reference time.
    const media::cast::RtpTimeTicks rtp_timestamp =
        last_frame_rtp_timestamp +
        std::max(media::cast::RtpTimeDelta::FromTicks(1),
                 media::cast::ToRtpTimeDelta(
                     remoting_frame->reference_time - last_frame_reference_time,
                     rtp_timebase_));
    remoting_frame->rtp_timestamp = rtp_timestamp;

    return remoting_frame;
  }

  int64_t frames_created() const { return frames_created_; }

 private:
  // The RTP timebase for this sender, set from the FrameSenderConfig.
  const int rtp_timebase_;

  // The frame sender for this frame creator.
  raw_ref<media::cast::FrameSender> const frame_sender_;

  // The clock.
  raw_ref<const base::TickClock> const clock_;

  // The total number of times CreateEncodedFrame() has been called.
  int64_t frames_created_ = 0;
};

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
                     std::move(error_callback)) {}

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
      decoder_buffer_reader_(std::make_unique<media::cast::DecoderBufferReader>(
          base::BindRepeating(&RemotingSender::OnFrameRead,
                              base::Unretained(this)),
          std::move(pipe))),
      stream_sender_(this, std::move(stream_sender)),
      is_audio_(config.rtp_payload_type <=
                media::cast::RtpPayloadType::REMOTE_AUDIO),
      frame_factory_(
          std::make_unique<SenderEncodedFrameFactory>(config.rtp_timebase,
                                                      *frame_sender_,
                                                      *clock_)) {
  stream_sender_.set_disconnect_handler(base::BindOnce(
      &RemotingSender::OnRemotingDataStreamError, base::Unretained(this)));

  // Eventually calls OnBufferRead().
  decoder_buffer_reader_->ReadBufferAsync();
}

RemotingSender::~RemotingSender() {
  const int64_t number_of_frames_inserted =
      static_cast<int64_t>(next_frame_id_ - media::cast::FrameId::first());
  const int64_t number_of_frames_created = frame_factory_->frames_created();

  // Record the number of frames dropped during this session.
  base::UmaHistogramPercentage(
      is_audio_ ? kHistogramDroppedAudioFrames : kHistogramDroppedVideoFrames,
      ((number_of_frames_created - number_of_frames_inserted) * 100) /
          std::max(int64_t{1}, number_of_frames_created));
}

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
  ClearCurrentFrame();
}

int RemotingSender::GetNumberOfFramesInEncoder() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

base::TimeDelta RemotingSender::GetEncoderBacklogDuration() const {
  NOTREACHED_IN_MIGRATION();
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

  // Create a new frame and exit early if it's invalid.
  auto remoting_frame =
      frame_factory_->CreateEncodedFrame(*next_frame_, next_frame_id_);
  if (!remoting_frame) {
    DLOG(WARNING) << "Invalid buffer received";
    ClearCurrentFrame();
    return;
  }

#if DCHECK_IS_ON()
  CHECK_GE(remoting_frame->referenced_frame_id, remoting_frame->frame_id - 1);
  if (flow_restart_pending_) {
    CHECK_EQ(remoting_frame->dependency, Dependency::kKeyFrame);
    CHECK_EQ(remoting_frame->referenced_frame_id, remoting_frame->frame_id);
  } else {
    CHECK_GT(remoting_frame->frame_id, media::cast::FrameId::first());
  }
#endif  // DCHECK_IS_ON()

  // Try to enqueue the frame and request a new frame be sent if the operation
  // succeeds.
  const auto rtp_timestamp = remoting_frame->rtp_timestamp;
  const media::cast::CastStreamingFrameDropReason reason =
      frame_sender_->EnqueueFrame(std::move(remoting_frame));
  if (reason == media::cast::CastStreamingFrameDropReason::kNotDropped) {
    DVLOG(1) << "Successfully sent frame " << next_frame_id_ << ".";

    flow_restart_pending_ = false;
    next_frame_id_++;
    consecutive_enqueue_frame_failure_count_ = 0;

    ClearCurrentFrame();
    return;
  } else {
    DLOG(WARNING) << "Dropped a frame for reason code="
                  << static_cast<int>(reason);
  }

  // If EnqueueFrame() has been failing repeatedly or the failure was due to the
  // formatting of the packet itself, drop it to avoid an infinite loop. Else,
  // retry. The "retry" case is the only case that should be hit during regular
  // execution of this function.
  //
  // By only dropping frames in such edge cases, all determinations about what
  // frames can / can't be dropped are delegated to the Renderer process and the
  // DemuxerStream::Read() function, rather than trying to "guess" here.
  if (++consecutive_enqueue_frame_failure_count_ > kMaxEnqueueFrameFailures) {
    DLOG(WARNING) << "Dropped frame due to repeated EnqueueFrame() failures.";
    ClearCurrentFrame();
  } else if (reason ==
             media::cast::CastStreamingFrameDropReason::kPayloadTooLarge) {
    DLOG(WARNING) << "Dropped frame with invalid formatting.";
    ClearCurrentFrame();
  } else {
    DVLOG(1) << "Failed to send frame " << next_frame_id_ << ". Retrying...";
  }

  base::UmaHistogramEnumeration(
      is_audio_ ? kHistogramAudioFrameDropped : kHistogramVideoFrameDropped,
      reason);
  TRACE_EVENT_INSTANT2(
      "cast.stream",
      is_audio_ ? "Remoting Audio Frame Drop" : "Remoting Video Frame Drop",
      TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp.lower_32_bits(),
      "reason", reason);
}

void RemotingSender::OnFrameRead(scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!next_frame_);
  DCHECK(read_complete_cb_);
  DCHECK(decoder_buffer_reader_);

  next_frame_ = std::move(buffer);

  // Legacy receivers expect the first frame to be a keyframe.
  if (flow_restart_pending_ && !next_frame_->end_of_stream()) {
    next_frame_->set_is_key_frame(true);
  }

  TrySendFrame();
}

void RemotingSender::OnRemotingDataStreamError() {
  // NOTE: This method must be idemptotent as it may be called more than once.
  decoder_buffer_reader_.reset();
  stream_sender_.reset();
  if (!error_callback_.is_null())
    std::move(error_callback_).Run();
}

void RemotingSender::ClearCurrentFrame() {
  if (!next_frame_) {
    return;
  }

  DCHECK(read_complete_cb_);
  DCHECK(!decoder_buffer_reader_->is_read_pending());
  next_frame_.reset();
  decoder_buffer_reader_->ReadBufferAsync();
  std::move(read_complete_cb_).Run();
}

}  // namespace mirroring
