// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/remoting_sender.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/sender/openscreen_frame_sender.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"
#include "third_party/openscreen/src/cast/streaming/sender.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace mirroring {

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
      data_pipe_reader_(new media::MojoDataPipeReader(std::move(pipe))),
      stream_sender_(this, std::move(stream_sender)),
      input_queue_discards_remaining_(0),
      is_reading_(false),
      flow_restart_pending_(true) {
  stream_sender_.set_disconnect_handler(base::BindOnce(
      &RemotingSender::OnRemotingDataStreamError, base::Unretained(this)));
}

RemotingSender::~RemotingSender() {}

void RemotingSender::SendFrame(uint32_t frame_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool need_to_start_processing = input_queue_.empty();
  input_queue_.push(base::BindRepeating(&RemotingSender::ReadFrame,
                                        base::Unretained(this), frame_size));
  input_queue_.push(base::BindRepeating(&RemotingSender::TrySendFrame,
                                        base::Unretained(this)));
  if (need_to_start_processing)
    ProcessNextInputTask();
}

void RemotingSender::CancelInFlightData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Flag that all pending input operations should discard data.
  input_queue_discards_remaining_ = input_queue_.size();
  flow_restart_pending_ = true;
  VLOG(1) << "Now restarting because in-flight data was just canceled.";
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
  // The frame cancellation may allow for the next input task to complete.
  ProcessNextInputTask();
}

void RemotingSender::ProcessNextInputTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (input_queue_.empty() || is_reading_)
    return;

  input_queue_.front().Run();
}

void RemotingSender::ReadFrame(uint32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_reading_);

  if (HadError()) {
    return;
  }
  if (!data_pipe_reader_->IsPipeValid()) {
    VLOG(1) << "Data pipe handle no longer valid.";
    OnRemotingDataStreamError();
    return;
  }

  is_reading_ = true;
  if (input_queue_discards_remaining_ > 0) {
    data_pipe_reader_->Read(
        nullptr, size,
        base::BindOnce(&RemotingSender::OnFrameRead, base::Unretained(this)));
  } else {
    next_frame_data_.resize(size);
    data_pipe_reader_->Read(
        reinterpret_cast<uint8_t*>(std::data(next_frame_data_)), size,
        base::BindOnce(&RemotingSender::OnFrameRead, base::Unretained(this)));
  }
}

void RemotingSender::TrySendFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_reading_);
  if (input_queue_discards_remaining_ > 0) {
    OnInputTaskComplete();
    return;
  }

  // If there would be too many frames in-flight, do not proceed.
  if (frame_sender_->GetUnacknowledgedFrameCount() >=
      media::cast::kMaxUnackedFrames) {
    VLOG(1) << "Cannot send frame now because too many frames are in flight.";
    return;
  }

  const bool is_first_frame = (next_frame_id_ == media::cast::FrameId::first());
  auto remoting_frame = std::make_unique<media::cast::SenderEncodedFrame>();
  remoting_frame->frame_id = next_frame_id_;
  if (flow_restart_pending_) {
    remoting_frame->dependency = Dependency::kKeyFrame;
    flow_restart_pending_ = false;
  } else {
    DCHECK(!is_first_frame);
    remoting_frame->dependency = Dependency::kDependent;
  }
  remoting_frame->referenced_frame_id =
      remoting_frame->dependency == Dependency::kKeyFrame ? next_frame_id_
                                                          : next_frame_id_ - 1;
  remoting_frame->reference_time = clock_->NowTicks();
  remoting_frame->encode_completion_time = remoting_frame->reference_time;

  base::TimeTicks last_frame_reference_time;
  media::cast::RtpTimeTicks last_frame_rtp_timestamp;
  if (is_first_frame) {
    last_frame_reference_time = remoting_frame->reference_time;
    last_frame_rtp_timestamp =
        media::cast::RtpTimeTicks() - media::cast::RtpTimeDelta::FromTicks(1);
  } else {
    last_frame_reference_time = frame_sender_->LastSendTime();
    last_frame_rtp_timestamp =
        frame_sender_->GetRecordedRtpTimestamp(next_frame_id_ - 1);
  }

  // Ensure each successive frame's RTP timestamp is unique, but otherwise just
  // base it on the reference time.
  const media::cast::RtpTimeTicks rtp_timestamp =
      last_frame_rtp_timestamp +
      std::max(media::cast::RtpTimeDelta::FromTicks(1),
               ToRtpTimeDelta(
                   remoting_frame->reference_time - last_frame_reference_time,
                   media::cast::kRemotingRtpTimebase));
  remoting_frame->rtp_timestamp = rtp_timestamp;
  remoting_frame->data.swap(next_frame_data_);

  if (frame_sender_->EnqueueFrame(std::move(remoting_frame))) {
    // Only increment if we successfully enqueued.
    next_frame_id_++;
  } else {
    TRACE_EVENT_INSTANT2("cast.stream", "Remoting Frame Drop",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         rtp_timestamp.lower_32_bits(), "reason",
                         "openscreen sender did not accept the frame");
  }
  OnInputTaskComplete();
}

void RemotingSender::OnFrameRead(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_reading_);
  is_reading_ = false;
  if (!success) {
    OnRemotingDataStreamError();
    return;
  }
  OnInputTaskComplete();
}

void RemotingSender::OnInputTaskComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!input_queue_.empty());
  input_queue_.pop();
  if (input_queue_discards_remaining_ > 0)
    --input_queue_discards_remaining_;

  // Always force a post task to prevent the stack from growing too deep.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RemotingSender::ProcessNextInputTask,
                                weak_factory_.GetWeakPtr()));
}

void RemotingSender::OnRemotingDataStreamError() {
  // NOTE: This method must be idemptotent as it may be called more than once.
  data_pipe_reader_.reset();
  stream_sender_.reset();
  if (!error_callback_.is_null())
    std::move(error_callback_).Run();
}

bool RemotingSender::HadError() const {
  DCHECK_EQ(!data_pipe_reader_, !stream_sender_.is_bound());
  return !data_pipe_reader_;
}

}  // namespace mirroring
