// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/public/feed_stream_surface.h"

namespace feed {
namespace {

void LogLaunchOtherStartForSurface(FeedStreamSurface* surface) {
  ReliabilityLoggingBridge& logger = surface->GetReliabilityLoggingBridge();
  logger.SendPendingLaunchEvents(surface->GetStreamType(),
                                 surface->GetSurfaceId());
  logger.LogFeedLaunchOtherStart(base::TimeTicks::Now());
}

}  // namespace

LaunchReliabilityLogger::LaunchReliabilityLogger(
    base::ObserverList<FeedStreamSurface>* surfaces)
    : surfaces_(surfaces) {}

LaunchReliabilityLogger::~LaunchReliabilityLogger() = default;

void LaunchReliabilityLogger::LogFeedLaunchOtherStart() {
  for (FeedStreamSurface& surface : *surfaces_)
    LogLaunchOtherStartForSurface(&surface);
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

}  // namespace feed