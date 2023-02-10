// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/prefs.h"

#include <utility>

#include "base/token.h"
#include "base/values.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace prefs {
namespace {

const char* RequestSchedulePrefName(RefreshTaskId task_id) {
  switch (task_id) {
    case feed::RefreshTaskId::kRefreshForYouFeed:
      return kRequestSchedule;
    case feed::RefreshTaskId::kRefreshWebFeed:
      return kWebFeedsRequestSchedule;
  }
}

}  // namespace
std::vector<int> GetThrottlerRequestCounts(PrefService& pref_service) {
  std::vector<int> result;
  const auto& value_list =
      pref_service.GetList(kThrottlerRequestCountListPrefName);
  for (const base::Value& value : value_list) {
    result.push_back(value.is_int() ? value.GetInt() : 0);
  }
  return result;
}

void SetThrottlerRequestCounts(std::vector<int> request_counts,
                               PrefService& pref_service) {
  base::Value::List value_list;
  for (int count : request_counts) {
    value_list.Append(count);
  }

  pref_service.SetList(kThrottlerRequestCountListPrefName,
                       std::move(value_list));
}

base::Time GetLastRequestTime(PrefService& pref_service) {
  return pref_service.GetTime(kThrottlerLastRequestTime);
}

void SetLastRequestTime(base::Time request_time, PrefService& pref_service) {
  return pref_service.SetTime(kThrottlerLastRequestTime, request_time);
}

DebugStreamData GetDebugStreamData(PrefService& pref_service) {
  return DeserializeDebugStreamData(pref_service.GetString(kDebugStreamData))
      .value_or(DebugStreamData());
}

void SetDebugStreamData(const DebugStreamData& data,
                        PrefService& pref_service) {
  pref_service.SetString(kDebugStreamData, SerializeDebugStreamData(data));
}

void SetRequestSchedule(RefreshTaskId task_id,
                        const RequestSchedule& schedule,
                        PrefService& pref_service) {
  pref_service.SetDict(RequestSchedulePrefName(task_id),
                       RequestScheduleToDict(schedule));
}

RequestSchedule GetRequestSchedule(RefreshTaskId task_id,
                                   PrefService& pref_service) {
  return RequestScheduleFromDict(
      pref_service.GetDict(RequestSchedulePrefName(task_id)));
}

void SetPersistentMetricsData(const PersistentMetricsData& data,
                              PrefService& pref_service) {
  pref_service.SetDict(kMetricsData, PersistentMetricsDataToDict(data));
}

PersistentMetricsData GetPersistentMetricsData(PrefService& pref_service) {
  return PersistentMetricsDataFromDict(pref_service.GetDict(kMetricsData));
}

std::string GetClientInstanceId(PrefService& pref_service) {
  std::string id = pref_service.GetString(feed::prefs::kClientInstanceId);
  if (!id.empty())
    return id;
  id = base::Token::CreateRandom().ToString();
  pref_service.SetString(feed::prefs::kClientInstanceId, id);
  return id;
}

void ClearClientInstanceId(PrefService& pref_service) {
  pref_service.ClearPref(feed::prefs::kClientInstanceId);
}

void SetWebFeedContentOrder(PrefService& pref_service,
                            ContentOrder content_order) {
  pref_service.SetInteger(feed::prefs::kWebFeedContentOrder,
                          static_cast<int>(content_order));
}

ContentOrder GetWebFeedContentOrder(const PrefService& pref_service) {
  int order = pref_service.GetInteger(feed::prefs::kWebFeedContentOrder);
  switch (order) {
    case static_cast<int>(ContentOrder::kReverseChron):
      return ContentOrder::kReverseChron;
    case static_cast<int>(ContentOrder::kGrouped):
      return ContentOrder::kGrouped;
    default:
      // Note: we need to handle invalid values gracefully.
      return ContentOrder::kUnspecified;
  }
}

}  // namespace prefs

}  // namespace feed
