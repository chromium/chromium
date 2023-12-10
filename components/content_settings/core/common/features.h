// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace content_settings {

#if BUILDFLAG(IS_ANDROID)
// Enables auto dark feature in theme settings.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kDarkenWebsitesCheckboxInThemesSetting);
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kDarkenWebsitesCheckboxOptOut;
#endif

namespace features {

// Feature to enable the unused site permissions module of Safety Check.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kSafetyCheckUnusedSitePermissions);

// Lets the HostContentSettingsMap actively monitor when content settings expire
// and delete them instantly. This also notifies observers that will, in turn,
// terminate access to capabilities gated on those settings right away.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kActiveContentSettingExpiry);

// Determines the frequency at which permissions of sites are checked whether
// they are unused.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRepeatedUpdateInterval;

// When enabled, site permissions will be considered as unused immediately in
// order to facilitate testing.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsNoDelay;

// When enabled, site permissions will be considered as unused after a smaller
// delay in order to facilitate testing.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool>
    kSafetyCheckUnusedSitePermissionsWithDelay;

// Determines the time interval after which sites are considered to be unused
// and its permissions will be revoked.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRevocationThreshold;

// Determines the time interval after which the revoked permissions of unused
// sites are cleaned up and no longer shown to users, starting from the point
// in time that permissions for a site were revoked.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold;

// Feature to enable the feedback button in the User Bypass UI.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kUserBypassFeedback);

// Feature to enable the User Bypass UI.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kUserBypassUI);

// Determines the time interval after which a user bypass exception expires.
// Note that it affects only new exceptions, previously created exceptions won't
// be updated to use a new expiration.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kUserBypassUIExceptionExpiration;

// Determines how many refreshes within `kUserBypassUIReloadTime` are required
// before a high confidence signal is returned.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<int> kUserBypassUIReloadCount;

// Determines how long a user has to make `kUserBypassUIReloadCount` refreshes
// before a high confidence signal is returned.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kUserBypassUIReloadTime;

// The reloading bubble will be shown until either the page full reloads or this
// timeout is reached.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kUserBypassUIReloadBubbleTimeout;

// Hide activity indicators if a permission is no longer used.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kImprovedSemanticsActivityIndicators);

// Feature to enable redesigned tracking protection UX + prefs for 3PCD.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kTrackingProtection3pcd);

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const char kTpcdReadHeuristicsGrantsName[];

// Enables writing and reading temporary storage access grants from 3PCD
// heuristics.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kTpcdHeuristicsGrants);

// Whether 3PCD heuristics grants should be considered to override cookie access
// behavior.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kTpcdReadHeuristicsGrants;

// Whether we should partition content settings (by StoragePartitions for
// non-ios platforms).
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kContentSettingsPartitioning);

// Enables writing and reading metadata grants as a host-indexed data structure.
// This is meant to optimize lookups when the list is large.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kHostIndexedMetadataGrants);

// The threshold for the amount of metadata grants needed to trigger usage of
// the optimized data structure.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<int> kMetadataGrantsThreshold;

}  // namespace features
}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_
