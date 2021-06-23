// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGING_BRIDGE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGING_BRIDGE_H_

#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/public/stream_type.h"

namespace feed {

/**
 * Interface for logging reliability-related timestamps and status codes. See
 * chrome/browser/xsurface/android/java/src/org/chromium/chrome/browser/
 * xsurface/FeedLaunchReliabilityLogger.java.
 */
class ReliabilityLoggingBridge {
 public:
  // Set stream metadata needed for logging and send any launch events that were
  // logged before the metadata was set.
  // Note: these methods correspond to FeedLaunchReliabilityLogger's
  // SendPendingEvents() and CancelPendingEvents(). "Launch" is added to their
  // names here because this interface is a bridge for all reliability logging
  // events, not just feed launch ones.
  virtual void SendPendingLaunchEvents(StreamType stream_type,
                                       SurfaceId stream_id) = 0;
  // Drop any pending events and reset the launch logger.
  virtual void CancelPendingLaunchEvents() = 0;

  // Methods for logging various events.
  virtual void LogFeedLaunchOtherStart(base::TimeTicks timestamp) = 0;

  // TODO(iwells): use
  virtual void LogCacheReadStart(base::TimeTicks timestamp) = 0;
  // TODO(iwells): use
  virtual void LogCacheReadEnd(
      base::TimeTicks timestamp,
      feedwire::DiscoverCardReadCacheResult result) = 0;

  // TODO(iwells): use
  virtual int LogFeedRequestStart(base::TimeTicks timestamp) = 0;
  // TODO(iwells): use
  virtual int LogActionsUploadRequestStart(base::TimeTicks timestamp) = 0;
  // TODO(iwells): use
  virtual void LogRequestSent(int request_id, base::TimeTicks timestamp) = 0;
  // TODO(iwells): use
  virtual void LogResponseReceived(
      int request_id,
      base::TimeTicks server_receive_timestamp,
      base::TimeTicks server_send_timestamp,
      base::TimeTicks client_receive_timestamp) = 0;
  // TODO(iwells): use
  virtual void LogRequestFinished(int request_id,
                                  base::TimeTicks timestamp,
                                  int combined_network_status_code) = 0;

  // TODO(iwells): use
  virtual void LogAtfRenderStart(base::TimeTicks timestamp) = 0;
  // TODO(iwells): use
  virtual void LogAtfRenderEnd(
      base::TimeTicks timestamp,
      feedwire::DiscoverAboveTheFoldRenderResult result) = 0;

  // TODO(iwells): log all remaining DiscoverLaunchResults
  virtual void LogLaunchFinished(base::TimeTicks timestamp,
                                 feedwire::DiscoverLaunchResult result) = 0;

  virtual ~ReliabilityLoggingBridge() = default;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGING_BRIDGE_H_