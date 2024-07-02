// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGING_BRIDGE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGING_BRIDGE_H_

#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/ios_shared_experiments_translator.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

/**
 * Interface for logging reliability-related timestamps and status codes. See
 * chrome/browser/xsurface/android/java/src/org/chromium/chrome/browser/
 * xsurface/feed/FeedLaunchReliabilityLogger.java.
 */
class ReliabilityLoggingBridge {
 public:
  // Methods for logging various events.
  virtual void LogFeedLaunchOtherStart(base::TimeTicks timestamp) = 0;

  virtual void LogCacheReadStart(base::TimeTicks timestamp) = 0;
  virtual void LogCacheReadEnd(
      base::TimeTicks timestamp,
      feedwire::DiscoverCardReadCacheResult result) = 0;

  virtual void LogFeedRequestStart(NetworkRequestId id,
                                   base::TimeTicks timestamp) = 0;
  virtual void LogActionsUploadRequestStart(NetworkRequestId id,
                                            base::TimeTicks timestamp) = 0;
  virtual void LogWebFeedRequestStart(NetworkRequestId id,
                                      base::TimeTicks timestamp) = 0;
  virtual void LogSingleWebFeedRequestStart(NetworkRequestId id,
                                            base::TimeTicks timestamp) = 0;
  virtual void LogRequestSent(NetworkRequestId id,
                              base::TimeTicks timestamp) = 0;
  virtual void LogResponseReceived(
      NetworkRequestId id,
      int64_t server_receive_timestamp_ns,
      int64_t server_send_timestamp_ns,
      base::TimeTicks client_receive_timestamp) = 0;
  virtual void LogRequestFinished(NetworkRequestId id,
                                  base::TimeTicks timestamp,
                                  int combined_network_status_code) = 0;

  virtual void LogLoadingIndicatorShown(base::TimeTicks timestamp) = 0;

  virtual void LogAboveTheFoldRender(
      base::TimeTicks timestamp,
      feedwire::DiscoverAboveTheFoldRenderResult result) = 0;

  virtual void LogLaunchFinishedAfterStreamUpdate(
      feedwire::DiscoverLaunchResult result) = 0;

  virtual void LogLoadMoreStarted() = 0;
  virtual void LogLoadMoreActionUploadRequestStarted() = 0;
  virtual void LogLoadMoreRequestSent() = 0;
  virtual void LogLoadMoreResponseReceived(
      int64_t server_receive_timestamp_ns,
      int64_t server_send_timestamp_ns) = 0;
  virtual void LogLoadMoreRequestFinished(int canonical_status) = 0;
  virtual void LogLoadMoreEnded(bool success) = 0;

  virtual void ReportExperiments(
      const std::vector<int32_t>& experiment_ids) = 0;

  virtual ~ReliabilityLoggingBridge() = default;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_RELIABILITY_LOGGING_BRIDGE_H_
