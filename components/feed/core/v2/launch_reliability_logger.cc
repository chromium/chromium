// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/launch_reliability_logger.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feed_stream_surface.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/stream_surface_set.h"

namespace feed {

LaunchReliabilityLogger::LaunchReliabilityLogger(StreamSurfaceSet* surfaces)
    : surfaces_(surfaces) {}

LaunchReliabilityLogger::~LaunchReliabilityLogger() = default;

void LaunchReliabilityLogger::LogFeedLaunchOtherStart() {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogFeedLaunchOtherStart(
        base::TimeTicks::Now());
  }
}

void LaunchReliabilityLogger::LogLaunchFinishedAfterStreamUpdate(
    feedwire::DiscoverLaunchResult result) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge()
        .LogLaunchFinishedAfterStreamUpdate(result);
  }
}

void LaunchReliabilityLogger::LogCacheReadStart() {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogCacheReadStart(
        base::TimeTicks::Now());
  }
}

void LaunchReliabilityLogger::LogCacheReadEnd(
    feedwire::DiscoverCardReadCacheResult result) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogCacheReadEnd(
        base::TimeTicks::Now(), result);
  }
}

NetworkRequestId LaunchReliabilityLogger::LogFeedRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogFeedRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

NetworkRequestId LaunchReliabilityLogger::LogActionsUploadRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogActionsUploadRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

NetworkRequestId LaunchReliabilityLogger::LogWebFeedRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogWebFeedRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

NetworkRequestId LaunchReliabilityLogger::LogSingleWebFeedRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogSingleWebFeedRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

void LaunchReliabilityLogger::LogRequestSent(NetworkRequestId id,
                                             base::TimeTicks timestamp) {
  for (auto& entry : *surfaces_)
    entry.renderer->GetReliabilityLoggingBridge().LogRequestSent(id, timestamp);
}

void LaunchReliabilityLogger::LogResponseReceived(
    NetworkRequestId id,
    int64_t server_receive_timestamp_ns,
    int64_t server_send_timestamp_ns,
    base::TimeTicks client_receive_timestamp) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogResponseReceived(
        id, server_receive_timestamp_ns, server_send_timestamp_ns,
        client_receive_timestamp);
  }
}

void LaunchReliabilityLogger::LogRequestFinished(
    NetworkRequestId id,
    int combined_network_status_code) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogRequestFinished(
        id, base::TimeTicks::Now(), combined_network_status_code);
  }
}

void LaunchReliabilityLogger::LogLoadMoreStarted() {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogLoadMoreStarted();
  }
}

void LaunchReliabilityLogger::LogLoadMoreActionUploadRequestStarted() {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge()
        .LogLoadMoreActionUploadRequestStarted();
  }
}

void LaunchReliabilityLogger::LogLoadMoreRequestSent() {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogLoadMoreRequestSent();
  }
}

void LaunchReliabilityLogger::LogLoadMoreResponseReceived(
    int64_t server_receive_timestamp_ns,
    int64_t server_send_timestamp_ns) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogLoadMoreResponseReceived(
        server_receive_timestamp_ns, server_send_timestamp_ns);
  }
}

void LaunchReliabilityLogger::LogLoadMoreRequestFinished(int canonical_status) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogLoadMoreRequestFinished(
        canonical_status);
  }
}

void LaunchReliabilityLogger::LogLoadMoreEnded(bool success) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().LogLoadMoreEnded(success);
  }
}

void LaunchReliabilityLogger::OnStreamUpdate(StreamUpdateType type) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    OnStreamUpdate(type, *entry.renderer);
  }
}

void LaunchReliabilityLogger::OnStreamUpdate(StreamUpdateType type,
                                             SurfaceRenderer& renderer) {
  ReliabilityLoggingBridge& logging_bridge =
      renderer.GetReliabilityLoggingBridge();
  switch (type) {
    case StreamUpdateType::kInitialLoadingSpinner:
      logging_bridge.LogLoadingIndicatorShown(base::TimeTicks::Now());
      break;
    case StreamUpdateType::kLoadingMoreSpinner:
      // Nothing to log. We will log only when the indicator is actuallly shown.
      break;
    case StreamUpdateType::kZeroState:
      logging_bridge.LogAboveTheFoldRender(
          base::TimeTicks::Now(),
          feedwire::DiscoverAboveTheFoldRenderResult::FULL_FEED_ERROR);
      break;
    case StreamUpdateType::kContent:
    case StreamUpdateType::kNone:
      logging_bridge.LogAboveTheFoldRender(
          base::TimeTicks::Now(),
          feedwire::DiscoverAboveTheFoldRenderResult::SUCCESS);
      break;
  }
}

void LaunchReliabilityLogger::ReportExperiments(
    const std::vector<int32_t>& experiment_ids) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.renderer->GetReliabilityLoggingBridge().ReportExperiments(
        experiment_ids);
  }
}

}  // namespace feed
