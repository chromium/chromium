// Copyright 2020 The Chromium Authors. All rights reserved.
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
      pref_service.GetList(kThrottlerRequestCountListPrefName)
          ->GetListDeprecated();
  for (const base::Value& value : value_list) {
    result.push_back(value.is_int() ? value.GetInt() : 0);
  }
  return result;
}

void SetThrottlerRequestCounts(std::vector<int> request_counts,
                               PrefService& pref_service) {
  std::vector<base::Value> value_list;
  for (int count : request_counts) {
    value_list.push_back(base::Value(count));
  }

  pref_service.Set(kThrottlerRequestCountListPrefName,
                   base::Value(std::move(value_list)));
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
  pref_service.Set(RequestSchedulePrefName(task_id),
                   RequestScheduleToValue(schedule));
}

RequestSchedule GetRequestSchedule(RefreshTaskId task_id,
                                   PrefService& pref_service) {
  return RequestScheduleFromValue(
      *pref_service.Get(RequestSchedulePrefName(task_id)));
}

void SetPersistentMetricsData(const PersistentMetricsData& data,
                              PrefService& pref_service) {
  pref_service.Set(kMetricsData, PersistentMetricsDataToValue(data));
}

PersistentMetricsData GetPersistentMetricsData(PrefService& pref_service) {
  return PersistentMetricsDataFromValue(*pref_service.Get(kMetricsData));
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

void SetExperiments(const Experiments& experiments, PrefService& pref_service) {
  base::Value value(base::Value::Type::DICTIONARY);
  for (const auto& exp : experiments) {
    value.SetStringKey(exp.first, exp.second);
  }
  pref_service.Set(kExperiments, value);
}

Experiments GetExperiments(PrefService& pref_service) {
  auto* value = pref_service.Get(kExperiments);
  Experiments experiments;
  if (!value->is_dict())
    return experiments;
  for (auto kv : value->DictItems()) {
    experiments[kv.first] = kv.second.GetString();
  }
  return experiments;
}

void SetWebFeedContentOrder(PrefService& pref_service,
                            ContentOrder content_order) {
  pref_service.Set(feed::prefs::kWebFeedContentOrder,
                   base::Value(static_cast<int>(content_order)));
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
