// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/scheduling.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/feed_feature_list.h"

namespace feed {
namespace {

base::Value::List VectorToList(const std::vector<base::TimeDelta>& values) {
  base::Value::List result;
  for (base::TimeDelta delta : values) {
    result.Append(base::TimeDeltaToValue(delta));
  }
  return result;
}

bool ListToVector(const base::Value::List& value,
                  std::vector<base::TimeDelta>* result) {
  for (const base::Value& entry : value) {
    std::optional<base::TimeDelta> delta = base::ValueToTimeDelta(entry);
    if (!delta)
      return false;
    result->push_back(*delta);
  }
  return true;
}

base::TimeDelta GetThresholdTime(base::TimeDelta default_threshold,
                                 base::TimeDelta server_threshold) {
  if (server_threshold <= base::TimeDelta() ||
      server_threshold > default_threshold) {
    return default_threshold;
  }
  return server_threshold;
}

RequestSchedule::Type GetScheduleType(const base::Value* value) {
  if (value && value->is_int()) {
    int int_value = value->GetInt();
    if (int_value >= 0 &&
        int_value <= base::to_underlying(RequestSchedule::Type::kMaxValue)) {
      return static_cast<RequestSchedule::Type>(int_value);
    }
  }
  return RequestSchedule::Type::kScheduledRefresh;
}

}  // namespace

RequestSchedule::RequestSchedule() = default;
RequestSchedule::~RequestSchedule() = default;
RequestSchedule::RequestSchedule(const RequestSchedule&) = default;
RequestSchedule& RequestSchedule::operator=(const RequestSchedule&) = default;
RequestSchedule::RequestSchedule(RequestSchedule&&) = default;
RequestSchedule& RequestSchedule::operator=(RequestSchedule&&) = default;

base::Value::Dict RequestScheduleToDict(const RequestSchedule& schedule) {
  base::Value::Dict result;
  result.Set("anchor", base::TimeToValue(schedule.anchor_time));
  result.Set("offsets", VectorToList(schedule.refresh_offsets));
  result.Set("type", base::to_underlying(schedule.type));
  return result;
}

RequestSchedule RequestScheduleFromDict(const base::Value::Dict& value) {
  RequestSchedule result;
  std::optional<base::Time> anchor = base::ValueToTime(value.Find("anchor"));
  const base::Value::List* offsets = value.FindList("offsets");
  result.type = GetScheduleType(value.Find("type"));

  if (!anchor || !offsets || !ListToVector(*offsets, &result.refresh_offsets))
    return {};
  result.anchor_time = *anchor;
  return result;
}

base::Time NextScheduledRequestTime(base::Time now, RequestSchedule* schedule) {
  if (schedule->refresh_offsets.empty())
    return now + GetFeedConfig().default_background_refresh_interval;
  // Attempt to detect system clock changes. If |anchor_time| is in the future,
  // or too far in the past, we reset |anchor_time| to now.
  if (now < schedule->anchor_time ||
      schedule->anchor_time + base::Days(7) < now) {
    schedule->anchor_time = now;
  }
  while (!schedule->refresh_offsets.empty()) {
    base::Time request_time =
        schedule->anchor_time + schedule->refresh_offsets[0];
    if (request_time <= now) {
      // The schedule time is in the past. This can happen if the scheduled
      // request already ran, or if the scheduled task was missed. Just ignore
      // this fetch so that we don't risk multiple fetches at a time.
      schedule->refresh_offsets.erase(schedule->refresh_offsets.begin());
      continue;
    }
    return request_time;
  }
  return now + GetFeedConfig().default_background_refresh_interval;
}

bool ShouldWaitForNewContent(const feedstore::Metadata& metadata,
                             const StreamType& stream_type,
                             base::TimeDelta content_age,
                             bool is_web_feed_subscriber) {
  const feedstore::Metadata::StreamMetadata* stream_metadata =
      feedstore::FindMetadataForStream(metadata, stream_type);
  if (stream_metadata && stream_metadata->is_known_stale())
    return true;

  base::TimeDelta staleness_threshold = GetFeedConfig().GetStalenessThreshold(
      stream_type, is_web_feed_subscriber);
  if (stream_metadata && stream_metadata->has_content_lifetime()) {
    staleness_threshold = GetThresholdTime(
        staleness_threshold,
        base::Milliseconds(stream_metadata->content_lifetime().stale_age_ms()));
  }

  return content_age > staleness_threshold;
}

bool ContentInvalidFromAge(const feedstore::Metadata& metadata,
                           const StreamType& stream_type,
                           base::TimeDelta content_age,
                           bool is_web_feed_subscriber) {
  const feedstore::Metadata::StreamMetadata* stream_metadata =
      feedstore::FindMetadataForStream(metadata, stream_type);

  base::TimeDelta content_expiration_threshold =
      GetFeedConfig().content_expiration_threshold;
  if (base::FeatureList::IsEnabled(kWebFeedOnboarding) &&
      !is_web_feed_subscriber && stream_type.IsWebFeed()) {
    content_expiration_threshold =
        GetFeedConfig().subscriptionless_content_expiration_threshold;
  }
  if (stream_metadata && stream_metadata->has_content_lifetime()) {
    content_expiration_threshold = GetThresholdTime(
        content_expiration_threshold,
        base::Milliseconds(
            stream_metadata->content_lifetime().invalid_age_ms()));
  }

  return content_age > content_expiration_threshold;
}

}  // namespace feed
