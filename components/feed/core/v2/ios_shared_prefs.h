// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_IOS_SHARED_PREFS_H_
#define COMPONENTS_FEED_CORE_V2_IOS_SHARED_PREFS_H_

#include <map>
#include <string>
#include <vector>

class PrefService;

namespace feed {

// A map of trial names (key) and list of group names/IDs (value)
// sent from the server.
typedef std::map<std::string, std::vector<std::string>> Experiments;

namespace prefs {
void SetLastFetchHadNoticeCard(PrefService& pref_service, bool value);
bool GetLastFetchHadNoticeCard(const PrefService& pref_service);

// TODO(b/213622639): These two functions are still used for iOS, but should
// be removed along with any calling code.
void SetHasReachedClickAndViewActionsUploadConditions(PrefService& pref_service,
                                                      bool value);
bool GetHasReachedClickAndViewActionsUploadConditions(
    const PrefService& pref_service);

// Increment the stored notice card views count by 1.
void IncrementNoticeCardViewsCount(PrefService& pref_service);
// Increment the stored notice card clicks count by 1.
void IncrementNoticeCardClicksCount(PrefService& pref_service);
int GetNoticeCardClicksCount(const PrefService& pref_service);
int GetNoticeCardViewsCount(const PrefService& pref_service);

// Set/get experiments into prefs.
void SetExperiments(const Experiments& experiments, PrefService& pref_service);
Experiments GetExperiments(PrefService& pref_service);
}  // namespace prefs
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_IOS_SHARED_PREFS_H_
