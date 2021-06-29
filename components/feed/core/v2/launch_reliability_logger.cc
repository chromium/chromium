// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/launch_reliability_logger.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/public/feed_stream_surface.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

LaunchReliabilityLogger::LaunchReliabilityLogger(
    base::ObserverList<FeedStreamSurface>* surfaces)
    : surfaces_(surfaces) {}

LaunchReliabilityLogger::~LaunchReliabilityLogger() = default;

void LaunchReliabilityLogger::LogFeedLaunchOtherStart() {
  for (FeedStreamSurface& surface : *surfaces_) {
    ReliabilityLoggingBridge& logger = surface.GetReliabilityLoggingBridge();
    logger.SendPendingLaunchEvents(surface.GetStreamType(),
                                   surface.GetSurfaceId());
    logger.LogFeedLaunchOtherStart(base::TimeTicks::Now());
  }
}

void LaunchReliabilityLogger::LogLaunchFinished(
    feedwire::DiscoverLaunchResult result) {
  for (FeedStreamSurface& surface : *surfaces_) {
    surface.GetReliabilityLoggingBridge().LogLaunchFinished(
        base::TimeTicks::Now(), result);
  }
}

void LaunchReliabilityLogger::LogCacheReadStart() {
  for (FeedStreamSurface& surface : *surfaces_) {
    surface.GetReliabilityLoggingBridge().LogCacheReadStart(
        base::TimeTicks::Now());
  }
}

void LaunchReliabilityLogger::LogCacheReadEnd(
    feedwire::DiscoverCardReadCacheResult result) {
  for (FeedStreamSurface& surface : *surfaces_) {
    surface.GetReliabilityLoggingBridge().LogCacheReadEnd(
        base::TimeTicks::Now(), result);
  }
}

NetworkRequestId LaunchReliabilityLogger::LogFeedRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (FeedStreamSurface& surface : *surfaces_) {
    surface.GetReliabilityLoggingBridge().LogFeedRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

NetworkRequestId LaunchReliabilityLogger::LogActionsUploadRequestStart() {
  NetworkRequestId id = request_id_gen_.GenerateNextId();
  for (FeedStreamSurface& surface : *surfaces_) {
    surface.GetReliabilityLoggingBridge().LogActionsUploadRequestStart(
        id, base::TimeTicks::Now());
  }
  return id;
}

void LaunchReliabilityLogger::LogRequestSent(NetworkRequestId id,
                                             base::TimeTicks timestamp) {
  for (FeedStreamSurface& surface : *surfaces_)
    surface.GetReliabilityLoggingBridge().LogRequestSent(id, timestamp);
}

void LaunchReliabilityLogger::LogResponseReceived(
    NetworkRequestId id,
    int64_t server_receive_timestamp_ns,
    int64_t server_send_timestamp_ns,
    base::TimeTicks client_receive_timestamp) {
  for (FeedStreamSurface& surface : *surfaces_) {
    surface.GetReliabilityLoggingBridge().LogResponseReceived(
        id, server_receive_timestamp_ns, server_send_timestamp_ns,
        client_receive_timestamp);
  }
}

void LaunchReliabilityLogger::LogRequestFinished(
    NetworkRequestId id,
    int combined_network_status_code) {
  for (FeedStreamSurface& surface : *surfaces_) {
    surface.GetReliabilityLoggingBridge().LogRequestFinished(
        id, base::TimeTicks::Now(), combined_network_status_code);
  }
}

}  // namespace feed