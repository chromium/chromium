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

// Enables early querying of storage access permissions to populate the renderer
// cache and eliminate synchronous IPCs during initial page load.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kEagerStorageAccessPermissionCheck);

// When enabled, site permissions will be considered as unused immediately in
// order to facilitate testing.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsNoDelay;

// When enabled, site permissions will be considered as unused after a smaller
// delay in order to facilitate testing.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool>
    kSafetyCheckUnusedSitePermissionsWithDelay;

// When enabled, allows users to choose between approximate and precise location
// in geolocation permission prompts.
//
// Enabling this feature will migrate geolocation permissions from
// ContentSettingsType::GEOLOCATION to
// ContentSettingsType::GEOLOCATION_WITH_OPTIONS. When the feature is enabled,
// ContentSettingType::GEOLOCATION_WITH_OPTIONS should be used in place of
// ContentSettingsType::GEOLOCATION. The correct ContentSettingsType for
// geolocation can always be retrieved using
// content_settings::GeolocationContentSettingsType().
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kApproximateGeolocationPermission);

// Move activity indicators to the left-hand side of Omnibox.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kLeftHandSideActivityIndicators);

// Move sensor activity indicators to the left-hand side of Omnibox.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kLeftHandSideSensorActivityIndicators);

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
BASE_DECLARE_FEATURE(kStorageAccessAPIRelatedWebsiteSets);

}  // namespace features
}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_
