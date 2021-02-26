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
      pref_service.GetList(kThrottlerRequestCountListPrefName)->GetList();
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

void SetLastFetchHadNoticeCard(PrefService& pref_service, bool value) {
  pref_service.SetBoolean(feed::prefs::kLastFetchHadNoticeCard, value);
}

bool GetLastFetchHadNoticeCard(const PrefService& pref_service) {
  return pref_service.GetBoolean(feed::prefs::kLastFetchHadNoticeCard);
}

void SetHasReachedClickAndViewActionsUploadConditions(PrefService& pref_service,
                                                      bool value) {
  pref_service.SetBoolean(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions, value);
}

bool GetHasReachedClickAndViewActionsUploadConditions(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions);
}

void IncrementNoticeCardViewsCount(PrefService& pref_service) {
  int count = pref_service.GetInteger(feed::prefs::kNoticeCardViewsCount);
  pref_service.SetInteger(feed::prefs::kNoticeCardViewsCount, count + 1);
}

int GetNoticeCardViewsCount(const PrefService& pref_service) {
  return pref_service.GetInteger(feed::prefs::kNoticeCardViewsCount);
}

void IncrementNoticeCardClicksCount(PrefService& pref_service) {
  int count = pref_service.GetInteger(feed::prefs::kNoticeCardClicksCount);
  pref_service.SetInteger(feed::prefs::kNoticeCardClicksCount, count + 1);
}

int GetNoticeCardClicksCount(const PrefService& pref_service) {
  return pref_service.GetInteger(feed::prefs::kNoticeCardClicksCount);
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
  for (const auto& kv : value->DictItems()) {
    experiments[kv.first] = kv.second.GetString();
  }
  return experiments;
}

}  // namespace prefs

}  // namespace feed
