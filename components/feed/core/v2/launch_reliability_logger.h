// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_LAUNCH_RELIABILITY_LOGGER_H_
#define COMPONENTS_FEED_CORE_V2_LAUNCH_RELIABILITY_LOGGER_H_

#include "base/observer_list.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/public/feed_stream_surface.h"

namespace feed {

class LaunchReliabilityLogger {
 public:
  explicit LaunchReliabilityLogger(
      base::ObserverList<FeedStreamSurface>* surfaces);
  ~LaunchReliabilityLogger();

  void LogFeedLaunchOtherStart();
  void LogLaunchFinished(feedwire::DiscoverLaunchResult result);

  void LogCacheReadStart();
  void LogCacheReadEnd(feedwire::DiscoverCardReadCacheResult result);

 private:
  base::ObserverList<FeedStreamSurface>* surfaces_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_LAUNCH_RELIABILITY_LOGGER_H_