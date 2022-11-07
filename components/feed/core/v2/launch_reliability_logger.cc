// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/launch_reliability_logger.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/public/feed_stream_surface.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/stream_surface_set.h"

namespace feed {

LaunchReliabilityLogger::LaunchReliabilityLogger(StreamSurfaceSet* surfaces)
    : surfaces_(surfaces) {}

LaunchReliabilityLogger::~LaunchReliabilityLogger() = default;

void LaunchReliabilityLogger::LogFeedLaunchOtherStart() {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogFeedLaunchOtherStart(
        base::TimeTicks::Now());
  }
}

void LaunchReliabilityLogger::LogLaunchFinishedAfterStreamUpdate(
    feedwire::DiscoverLaunchResult result) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge()
        .LogLaunchFinishedAfterStreamUpdate(result);
  }
}

void LaunchReliabilityLogger::LogCacheReadStart() {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogCacheReadStart(
        base::TimeTicks::Now());
  }
}

void LaunchReliabilityLogger::LogCacheReadEnd(
    feedwire::DiscoverCardReadCacheResult result) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogCacheReadEnd(
        base::TimeTicks::Now(), result);
  }
}

NetworkRequestId LaunchReliabilityLogger::LogFeedRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogFeedRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

NetworkRequestId LaunchReliabilityLogger::LogActionsUploadRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogActionsUploadRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

NetworkRequestId LaunchReliabilityLogger::LogWebFeedRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogWebFeedRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

NetworkRequestId LaunchReliabilityLogger::LogSingleWebFeedRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogSingleWebFeedRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

void LaunchReliabilityLogger::LogRequestSent(NetworkRequestId id,
                                             base::TimeTicks timestamp) {
  for (auto& entry : *surfaces_)
    entry.surface->GetReliabilityLoggingBridge().LogRequestSent(id, timestamp);
}

void LaunchReliabilityLogger::LogResponseReceived(
    NetworkRequestId id,
    int64_t server_receive_timestamp_ns,
    int64_t server_send_timestamp_ns,
    base::TimeTicks client_receive_timestamp) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogResponseReceived(
        id, server_receive_timestamp_ns, server_send_timestamp_ns,
        client_receive_timestamp);
  }
}

void LaunchReliabilityLogger::LogRequestFinished(
    NetworkRequestId id,
    int combined_network_status_code) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces()) {
    entry.surface->GetReliabilityLoggingBridge().LogRequestFinished(
        id, base::TimeTicks::Now(), combined_network_status_code);
  }
}

void LaunchReliabilityLogger::OnStreamUpdate(StreamUpdateType type) {
  for (const StreamSurfaceSet::Entry& entry : surfaces_->surfaces())
    OnStreamUpdate(type, *entry.surface);
}

void LaunchReliabilityLogger::OnStreamUpdate(StreamUpdateType type,
                                             FeedStreamSurface& surface) {
  ReliabilityLoggingBridge& logging_bridge =
      surface.GetReliabilityLoggingBridge();
  switch (type) {
    case StreamUpdateType::kInitialLoadingSpinner:
      logging_bridge.LogLoadingIndicatorShown(base::TimeTicks::Now());
      break;
    case StreamUpdateType::kLoadingMoreSpinner:
      // TODO(iwells): log this with next-page flow
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

}  // namespace feed
