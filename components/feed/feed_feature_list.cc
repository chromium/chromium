// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"

namespace feed {

const base::Feature kInterestFeedContentSuggestions{
    "InterestFeedContentSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kDisableTriggerTypes{
    &kInterestFeedContentSuggestions, "disable_trigger_types", ""};
const base::FeatureParam<int> kSuppressRefreshDurationMinutes{
    &kInterestFeedContentSuggestions, "suppress_refresh_duration_minutes", 30};
const base::FeatureParam<int> kTimeoutDurationSeconds{
    &kInterestFeedContentSuggestions, "timeout_duration_seconds", 30};
const base::FeatureParam<bool> kThrottleBackgroundFetches{
    &kInterestFeedContentSuggestions, "throttle_background_fetches", true};
const base::FeatureParam<bool> kOnlySetLastRefreshAttemptOnSuccess{
    &kInterestFeedContentSuggestions,
    "only_set_last_refresh_attempt_on_success", true};

const base::Feature kInterestFeedNotifications{
    "InterestFeedNotifications", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace feed
