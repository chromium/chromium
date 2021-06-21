// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGER_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGER_H_

#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/public/stream_type.h"

namespace feed {

/**
 * Interface for logging reliability-related timestamps and status codes. See
 * chrome/browser/xsurface/android/java/src/org/chromium/chrome/browser/
 * xsurface/FeedLaunchReliabilityLogger.java.
 */
class ReliabilityLogger {
 public:
  // Set stream metadata needed for logging and send any launch events that were
  // logged before the metadata was set.
  virtual void SendPendingLaunchEvents(StreamType stream_type,
                                       SurfaceId stream_id) = 0;
  // Drop any pending events and reset the launch logger.
  virtual void CancelPendingLaunchEvents() = 0;

  // Methods for logging various events.
  virtual void LogCacheReadStart(base::TimeTicks timestamp) = 0;
  virtual void LogCacheReadEnd(
      base::TimeTicks timestamp,
      feedwire::DiscoverCardReadCacheResult result) = 0;

  virtual int LogFeedRequestStart(base::TimeTicks timestamp) = 0;
  virtual int LogActionsUploadRequestStart(base::TimeTicks timestamp) = 0;
  virtual void LogRequestSent(int request_id, base::TimeTicks timestamp) = 0;
  virtual void LogResponseReceived(
      int request_id,
      base::TimeTicks server_receive_timestamp,
      base::TimeTicks server_send_timestamp,
      base::TimeTicks client_receive_timestamp) = 0;
  virtual void LogRequestFinished(int request_id,
                                  base::TimeTicks timestamp,
                                  int combined_network_status_code) = 0;

  virtual void LogAtfRenderStart(base::TimeTicks timestamp) = 0;
  virtual void LogAtfRenderEnd(
      base::TimeTicks timestamp,
      feedwire::DiscoverAboveTheFoldRenderResult result) = 0;

  virtual void LogLaunchFinished(base::TimeTicks timestamp,
                                 feedwire::DiscoverLaunchResult result) = 0;

  virtual ~ReliabilityLogger() = default;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGER_H_