// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PREFS_H_
#define COMPONENTS_FEED_CORE_V2_PREFS_H_

#include <vector>

#include "base/time/time.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/types.h"

class PrefService;

namespace feed {
struct RequestSchedule;
namespace prefs {

// Functions for accessing prefs.

// For counting previously made requests, one integer for each
// |NetworkRequestType|.
std::vector<int> GetThrottlerRequestCounts(PrefService& pref_service);
void SetThrottlerRequestCounts(std::vector<int> request_counts,
                               PrefService& pref_service);

// Time of the last request. For determining whether the next day's quota should
// be released.
base::Time GetLastRequestTime(PrefService& pref_service);
void SetLastRequestTime(base::Time request_time, PrefService& pref_service);

DebugStreamData GetDebugStreamData(PrefService& pref_service);
void SetDebugStreamData(const DebugStreamData& data, PrefService& pref_service);

void SetRequestSchedule(RefreshTaskId task_id,
                        const RequestSchedule& schedule,
                        PrefService& pref_service);
RequestSchedule GetRequestSchedule(RefreshTaskId task_id,
                                   PrefService& pref_service);

PersistentMetricsData GetPersistentMetricsData(PrefService& pref_service);
void SetPersistentMetricsData(const PersistentMetricsData& data,
                              PrefService& pref_service);

std::string GetClientInstanceId(PrefService& pref_service);
void ClearClientInstanceId(PrefService& pref_service);

void SetWebFeedContentOrder(PrefService& pref_service,
                            ContentOrder content_order);
ContentOrder GetWebFeedContentOrder(const PrefService& pref_service);

}  // namespace prefs
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PREFS_H_
