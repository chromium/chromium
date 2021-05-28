// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_PREFS_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_PREFS_H_

class PrefService;

namespace ios_feed {
namespace prefs {

void SetLastFetchHadNoticeCard(PrefService& pref_service, bool value);
bool GetLastFetchHadNoticeCard(const PrefService& pref_service);
void SetHasReachedClickAndViewActionsUploadConditions(PrefService& pref_service,
                                                      bool value);
bool GetHasReachedClickAndViewActionsUploadConditions(
    const PrefService& pref_service);

}  // namespace prefs
}  // namespace ios_feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_PREFS_H_
