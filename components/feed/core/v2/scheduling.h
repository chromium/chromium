// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_SCHEDULING_H_
#define COMPONENTS_FEED_CORE_V2_SCHEDULING_H_

#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/public/stream_type.h"

namespace feedstore {
class Metadata;
}
namespace feed {
// A schedule for making Feed refresh requests.
// |anchor_time| + |refresh_offsets[i]| is the time each fetch should be made.
struct RequestSchedule {
  RequestSchedule();
  ~RequestSchedule();
  RequestSchedule(const RequestSchedule&);
  RequestSchedule& operator=(const RequestSchedule&);
  RequestSchedule(RequestSchedule&&);
  RequestSchedule& operator=(RequestSchedule&&);

  enum class Type : int {
    kScheduledRefresh = 0,
    kFeedCloseRefresh = 1,
    kMaxValue = kFeedCloseRefresh,
  };

  base::Time anchor_time;
  std::vector<base::TimeDelta> refresh_offsets;
  Type type = Type::kScheduledRefresh;
};

base::Value::Dict RequestScheduleToDict(const RequestSchedule& schedule);
RequestSchedule RequestScheduleFromDict(const base::Value::Dict& dict);
// Given a schedule, returns the next time a request should be made.
// Updates |schedule| accordingly. If |schedule| has no fetches remaining,
// returns a scheduled time using |Config::default_background_refresh_interval|.
base::Time NextScheduledRequestTime(base::Time now, RequestSchedule* schedule);

// Returns whether we should wait for new content before showing stream content.
bool ShouldWaitForNewContent(const feedstore::Metadata& metadata,
                             const StreamType& stream_type,
                             base::TimeDelta content_age,
                             bool is_web_feed_subscriber);

bool ContentInvalidFromAge(const feedstore::Metadata& metadata,
                           const StreamType& stream_type,
                           base::TimeDelta content_age,
                           bool is_web_feed_subscriber);
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_SCHEDULING_H_
