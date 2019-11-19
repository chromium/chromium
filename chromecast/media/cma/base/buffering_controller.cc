// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/buffering_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

namespace {

// Maximum time for buffering before we error out the stream.
constexpr base::TimeDelta kBufferingTimeout = base::TimeDelta::FromMinutes(1);

}  // namespace

BufferingController::BufferingController(
    const scoped_refptr<BufferingConfig>& config,
    const BufferingNotificationCB& buffering_notification_cb)
    : config_(config),
      buffering_notification_cb_(buffering_notification_cb),
      is_buffering_(false),
      begin_buffering_time_(base::Time()),
      last_buffer_end_time_(base::Time()),
      initial_buffering_(true),
      buffering_timeout_exceeded_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
  LOG(INFO) << __FUNCTION__
            << " High threshold: " << config_->high_level().InMilliseconds()
            << "ms Low threshold: " << config_->low_level().InMilliseconds()
            << "ms";
}

BufferingController::~BufferingController() {
  // Some weak pointers might possibly be invalidated here.
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BufferingController::UpdateHighLevelThreshold(
    base::TimeDelta high_level_threshold) {
  // Can only decrease the high level threshold.
  if (high_level_threshold > config_->high_level())
    return;
  LOG(INFO) << "High buffer threshold: "
            << high_level_threshold.InMilliseconds() << "ms";
  config_->set_high_level(high_level_threshold);

  // Make sure the low level threshold is somewhat consistent.
  // Currently, we set it to one third of the high level threshold:
  // this value could be adjusted in the future.
  base::TimeDelta low_level_threshold = high_level_threshold / 3;
  if (low_level_threshold <= config_->low_level()) {
    LOG(INFO) << "Low buffer threshold: "
              << low_level_threshold.InMilliseconds() << "ms";
    config_->set_low_level(low_level_threshold);
  }

  // Signal all the streams the config has changed.
  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    (*it)->OnConfigChanged();
  }

  // Once all the streams have been notified, the buffering state must be
  // updated (no notification is received from the streams).
  OnBufferingStateChanged(false, false);
}

scoped_refptr<BufferingState> BufferingController::AddStream(
    const std::string& stream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Add a new stream to the list of streams being monitored.
  scoped_refptr<BufferingState> buffering_state(new BufferingState(
      stream_id,
      config_,
      base::Bind(&BufferingController::OnBufferingStateChanged, weak_this_,
                 false, false),
      base::Bind(&BufferingController::UpdateHighLevelThreshold, weak_this_)));
  stream_list_.push_back(buffering_state);

  // Update the state and force a notification to the streams.
  // TODO(damienv): Should this be a PostTask ?
  OnBufferingStateChanged(true, false);

  return buffering_state;
}

void BufferingController::SetMediaTime(base::TimeDelta time) {
  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    (*it)->SetMediaTime(time);
  }
}

base::TimeDelta BufferingController::GetMaxRenderingTime() const {
  base::TimeDelta max_rendering_time(::media::kNoTimestamp);
  for (StreamList::const_iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    base::TimeDelta max_stream_rendering_time =
        (*it)->GetMaxRenderingTime();
    if (max_stream_rendering_time == ::media::kNoTimestamp)
      return ::media::kNoTimestamp;
    if (max_rendering_time == ::media::kNoTimestamp ||
        max_stream_rendering_time < max_rendering_time) {
      max_rendering_time = max_stream_rendering_time;
    }
  }
  return max_rendering_time;
}

void BufferingController::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_buffering_ = false;
  initial_buffering_ = true;
  buffering_timeout_exceeded_ = false;
  buffering_timer_.Stop();
  stream_list_.clear();
}

void BufferingController::OnBufferingStateChanged(
    bool force_notification, bool buffering_timeout) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Log the state of each stream.
  DumpState();

  bool is_low_buffering = IsLowBufferLevel();
  bool is_high_buffering = !is_low_buffering;
  if (!buffering_timeout) {
    // Hysteresis:
    // - to leave buffering, not only should we leave the low buffer level state
    //   but we should go to the high buffer level state (medium is not enough).
    is_high_buffering = IsHighBufferLevel();
  }

  bool is_buffering_prv = is_buffering_;
  if (is_buffering_) {
    if (is_high_buffering)
      is_buffering_ = false;
  } else {
    if (is_low_buffering)
      is_buffering_ = true;
  }

  // Start buffering.
  if (is_buffering_ && !is_buffering_prv) {
    begin_buffering_time_ = base::Time::Now();
    buffering_timer_.Start(FROM_HERE, kBufferingTimeout, this,
                           &BufferingController::BufferingTimeoutExceeded);
  }

  // End buffering.
  if (is_buffering_prv && !is_buffering_) {
    base::Time current_time = base::Time::Now();
    base::TimeDelta buffering_user_time = current_time - begin_buffering_time_;
    chromecast::metrics::CastMetricsHelper* metrics_helper =
        chromecast::metrics::CastMetricsHelper::GetInstance();
    LOG(INFO) << "Buffering took: " << buffering_user_time.InMilliseconds()
              << "ms";
    chromecast::metrics::CastMetricsHelper::BufferingType buffering_type =
        initial_buffering_ ?
            chromecast::metrics::CastMetricsHelper::kInitialBuffering :
            chromecast::metrics::CastMetricsHelper::kBufferingAfterUnderrun;
    metrics_helper->LogTimeToBufferAv(buffering_type, buffering_user_time);

    if (!initial_buffering_) {
      base::TimeDelta time_between_buffering =
          begin_buffering_time_ - last_buffer_end_time_;
      LOG(INFO) << "Time since last buffering event: "
                << time_between_buffering.InMilliseconds() << "ms";
      metrics_helper->RecordApplicationEventWithValue(
          "Cast.Platform.PlayTimeBeforeAutoPause",
          time_between_buffering.InMilliseconds());
      metrics_helper->RecordApplicationEventWithValue(
          "Cast.Platform.AutoPauseTime", buffering_user_time.InMilliseconds());
    }
    // Only the first buffering report is considered "initial buffering".
    last_buffer_end_time_ = current_time;
    initial_buffering_ = false;
    buffering_timer_.Stop();
  }

  // Don't notify any buffering change if the timeout was exceeded, to avoid
  // user surprise if playback resumes after extremely long buffering.
  if (!buffering_timeout_exceeded_ &&
      (is_buffering_prv != is_buffering_ || force_notification))
    buffering_notification_cb_.Run(is_buffering_);
}

void BufferingController::BufferingTimeoutExceeded() {
  LOG(INFO) << __FUNCTION__;
  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
      "Cast.Platform.BufferingTimeoutExceeded");
  buffering_timeout_exceeded_ = true;
}

bool BufferingController::IsHighBufferLevel() {
  if (stream_list_.empty())
    return true;

  bool is_high_buffering = true;
  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    BufferingState::State stream_state = (*it)->GetState();
    is_high_buffering = is_high_buffering &&
        ((stream_state == BufferingState::kHighLevel) ||
         (stream_state == BufferingState::kEosReached));
  }
  return is_high_buffering;
}

bool BufferingController::IsLowBufferLevel() {
  if (stream_list_.empty())
    return false;

  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    BufferingState::State stream_state = (*it)->GetState();
    if (stream_state == BufferingState::kLowLevel)
      return true;
  }

  return false;
}

void BufferingController::DumpState() const {
  for (StreamList::const_iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    LOG(INFO) << (*it)->ToString();
  }
}

}  // namespace media
}  // namespace chromecast
