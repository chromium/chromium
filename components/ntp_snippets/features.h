// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_FEATURES_H_
#define COMPONENTS_NTP_SNIPPETS_FEATURES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/prefs/pref_service.h"

namespace base {
class Clock;
}

namespace ntp_snippets {

//
// Null-terminated list of all features related to content suggestions.
//
// If you add a base::Feature below, you must add it to this list. It is used in
// internal pages to list relevant parameters and settings.
//
extern const base::Feature* const kAllFeatures[];

////////////////////////////////////////////////////////////////////////////////
// Independent features. Treat as normal

extern const base::Feature kArticleSuggestionsFeature;

// Feature for simple experimental comparison and validation of changes since
// M58: enabling this brings back the M58 Stable fetching schedule (which is
// suitable for Holdback groups).
// TODO(jkrcal): Remove when the comparison is done (probably after M62).
extern const base::Feature kRemoteSuggestionsEmulateM58FetchingSchedule;

std::unique_ptr<CategoryRanker> BuildSelectedCategoryRanker(
    PrefService* pref_service,
    base::Clock* clock);

// Enables and configures notifications for content suggestions on Android.
extern const base::Feature kNotificationsFeature;

// An integer. The priority of the notification, ranging from -2 (PRIORITY_MIN)
// to 2 (PRIORITY_MAX). Vibrates and makes sound if >= 0.
extern const char kNotificationsPriorityParam[];
constexpr int kNotificationsDefaultPriority = -1;

// "publisher": use article's publisher as notification's text (default).
// "snippet": use article's snippet as notification's text.
// "and_more": use "From $1. Read this article and $2 more." as text.
extern const char kNotificationsTextParam[];
extern const char kNotificationsTextValuePublisher[];
extern const char kNotificationsTextValueSnippet[];
extern const char kNotificationsTextValueAndMore[];

// "true": when Chrome becomes frontmost, leave notifications open.
// "false": automatically dismiss notification when Chrome becomes frontmost.
extern const char kNotificationsKeepWhenFrontmostParam[];

// "true": notifications link to chrome://newtab, with appropriate text.
// "false": notifications link to URL of notifying article.
extern const char kNotificationsOpenToNTPParam[];

// An integer. The maximum number of notifications that will be shown in 1 day.
extern const char kNotificationsDailyLimit[];
constexpr int kNotificationsDefaultDailyLimit = 1;

// An integer. The number of notifications that can be ignored. If the user
// ignores this many notifications or more, we stop sending them.
extern const char kNotificationsIgnoredLimitParam[];
constexpr int kNotificationsIgnoredDefaultLimit = 3;

// Whether to keep some prefetched content suggestions even when new suggestions
// have been fetched.
extern const base::Feature kKeepPrefetchedContentSuggestions;

// Whether this version of the client supports responses without an image.
extern const base::Feature kOptionalImagesEnabledFeature;

// Return all the features as a vector.
std::vector<const base::Feature*> GetAllFeatures();

// Return a referrer URL for content suggestions.
std::string GetContentSuggestionsReferrerURL();
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_FEATURES_H_
