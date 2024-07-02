// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_LAUNCH_RELIABILITY_LOGGER_H_
#define COMPONENTS_FEED_CORE_V2_LAUNCH_RELIABILITY_LOGGER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/ios_shared_experiments_translator.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/surface_renderer.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {
class StreamSurfaceSet;

class LaunchReliabilityLogger {
 public:
  explicit LaunchReliabilityLogger(StreamSurfaceSet* surfaces);
  ~LaunchReliabilityLogger();

  void LogFeedLaunchOtherStart();

  void LogCacheReadStart();
  void LogCacheReadEnd(feedwire::DiscoverCardReadCacheResult result);

  // TODO(iwells): Move the network events to their own logger class when
  // implementing logging for the next page flow.

  NetworkRequestId LogFeedRequestStart();
  NetworkRequestId LogActionsUploadRequestStart();
  NetworkRequestId LogWebFeedRequestStart();
  NetworkRequestId LogSingleWebFeedRequestStart();
  void LogRequestSent(NetworkRequestId id, base::TimeTicks timestamp);
  void LogResponseReceived(NetworkRequestId id,
                           int64_t server_receive_timestamp_ns,
                           int64_t server_send_timestamp_ns,
                           base::TimeTicks client_receive_timestamp);
  void LogRequestFinished(NetworkRequestId id,
                          int combined_network_status_code);

  void LogLoadMoreStarted();
  void LogLoadMoreActionUploadRequestStarted();
  void LogLoadMoreRequestSent();
  void LogLoadMoreResponseReceived(int64_t server_receive_timestamp_ns,
                                   int64_t server_send_timestamp_ns);
  void LogLoadMoreRequestFinished(int combined_network_status_code);
  void LogLoadMoreEnded(bool success);

  enum class StreamUpdateType {
    kNone,
    kInitialLoadingSpinner,
    kLoadingMoreSpinner,
    kZeroState,
    kContent,
  };

  // Logs "above-the-fold render" or "loading indicator shown"
  // depending on update type. Should be called just
  // before sending each stream update during launch.
  void OnStreamUpdate(StreamUpdateType type);
  void OnStreamUpdate(StreamUpdateType type, SurfaceRenderer& renderer);

  void LogLaunchFinishedAfterStreamUpdate(
      feedwire::DiscoverLaunchResult result);

  void ReportExperiments(const std::vector<int32_t>& experiment_ids);

 private:
  raw_ptr<StreamSurfaceSet, DanglingUntriaged> surfaces_;
  NetworkRequestId::Generator request_id_gen_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_LAUNCH_RELIABILITY_LOGGER_H_
