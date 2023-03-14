// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_SESSION_LOGGER_H_
#define COMPONENTS_MIRRORING_SERVICE_SESSION_LOGGER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/log_event_dispatcher.h"
#include "media/cast/logging/receiver_time_offset_estimator.h"
#include "media/cast/logging/stats_event_subscriber.h"

class LogEventDispatcher;

namespace mirroring {

// Handles logging and statistics of a mirroring session.
class COMPONENT_EXPORT(MIRRORING_SERVICE) SessionLogger {
 public:
  explicit SessionLogger(
      scoped_refptr<media::cast::CastEnvironment> cast_environment);

  // Constructor used for testing.
  SessionLogger(scoped_refptr<media::cast::CastEnvironment> cast_environment,
                std::unique_ptr<media::cast::ReceiverTimeOffsetEstimator>
                    offset_estimator);

  SessionLogger(const SessionLogger&) = delete;
  SessionLogger& operator=(const SessionLogger&) = delete;

  virtual ~SessionLogger();

  // Returns a dictionary containing statistics for the current session. The
  // dictionary contains two entries - "audio" or "video" pointing to an inner
  // dictionary. The inner dictionary consists of string - double entries, where
  // the string describes the name of the stat, and the double describes the
  // value of the stat. See CastStat and StatsMap of the StatsEventSubscriber
  // object for more details.
  base::Value::Dict GetStats() const;

 protected:
  void SubscribeToLoggingEvents(media::cast::LogEventDispatcher& logger);
  void UnsubscribeFromLoggingEvents(media::cast::LogEventDispatcher& logger);

  scoped_refptr<media::cast::CastEnvironment> cast_environment_;

  std::unique_ptr<media::cast::ReceiverTimeOffsetEstimator> offset_estimator_;
  media::cast::StatsEventSubscriber video_stats_subscriber_;
  media::cast::StatsEventSubscriber audio_stats_subscriber_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_SESSION_LOGGER_H_
