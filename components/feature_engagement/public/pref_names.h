// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_PREF_NAMES_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_PREF_NAMES_H_

class PrefRegistrySimple;

namespace feature_engagement {

inline constexpr char kFeatureEngagementProfileToDeviceMigrationCompleted[] =
    "feature_engagement.profile_to_device_event_migration_completed";

// Register preference names for feature engagement features.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_PREF_NAMES_H_
