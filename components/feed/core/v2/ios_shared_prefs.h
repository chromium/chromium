// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_IOS_SHARED_PREFS_H_
#define COMPONENTS_FEED_CORE_V2_IOS_SHARED_PREFS_H_

#include "components/feed/core/v2/ios_shared_experiments_translator.h"

class PrefService;

namespace feed {

using ::feed::Experiments;

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

void MigrateObsoleteFeedExperimentPref_Jun_2024(PrefService* prefs);

}  // namespace prefs
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_IOS_SHARED_PREFS_H_
