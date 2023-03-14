// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session_logger.h"

#include "media/cast/logging/receiver_time_offset_estimator_impl.h"

namespace mirroring {

SessionLogger::SessionLogger(
    scoped_refptr<media::cast::CastEnvironment> cast_environment)
    : SessionLogger(
          cast_environment,
          std::make_unique<media::cast::ReceiverTimeOffsetEstimatorImpl>()) {}

SessionLogger::SessionLogger(
    scoped_refptr<media::cast::CastEnvironment> cast_environment,
    std::unique_ptr<media::cast::ReceiverTimeOffsetEstimator> offset_estimator)
    : cast_environment_(cast_environment),
      offset_estimator_(std::move(offset_estimator)),
      video_stats_subscriber_(media::cast::VIDEO_EVENT,
                              cast_environment->Clock(),
                              offset_estimator_.get()),
      audio_stats_subscriber_(media::cast::AUDIO_EVENT,
                              cast_environment->Clock(),
                              offset_estimator_.get()) {
  auto* logger = cast_environment_->logger();
  if (logger) {
    SubscribeToLoggingEvents(*logger);
  }
}

SessionLogger::~SessionLogger() {
  auto* logger = cast_environment_->logger();
  if (logger) {
    UnsubscribeFromLoggingEvents(*logger);
  }
}

base::Value::Dict SessionLogger::GetStats() const {
  base::Value::Dict video_dict = video_stats_subscriber_.GetStats();
  base::Value::Dict audio_dict = audio_stats_subscriber_.GetStats();

  base::Value::Dict combined_stats;
  base::Value::Dict* audio_dict_value = audio_dict.FindDict(
      media::cast::StatsEventSubscriber::kAudioStatsDictKey);
  if (audio_dict_value) {
    combined_stats.Set(media::cast::StatsEventSubscriber::kAudioStatsDictKey,
                       std::move(*audio_dict_value));
  }

  base::Value::Dict* video_dict_value = video_dict.FindDict(
      media::cast::StatsEventSubscriber::kVideoStatsDictKey);
  if (video_dict_value) {
    combined_stats.Set(media::cast::StatsEventSubscriber::kVideoStatsDictKey,
                       std::move(*video_dict_value));
  }

  return combined_stats;
}

void SessionLogger::SubscribeToLoggingEvents(
    media::cast::LogEventDispatcher& logger) {
  logger.Subscribe(offset_estimator_.get());
  logger.Subscribe(&video_stats_subscriber_);
  logger.Subscribe(&audio_stats_subscriber_);
}

void SessionLogger::UnsubscribeFromLoggingEvents(
    media::cast::LogEventDispatcher& logger) {
  logger.Unsubscribe(&video_stats_subscriber_);
  logger.Unsubscribe(&audio_stats_subscriber_);
  logger.Unsubscribe(offset_estimator_.get());
}

}  // namespace mirroring
