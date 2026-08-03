// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content_settings {

// Enables auto dark feature in theme settings.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kDarkenWebsitesCheckboxInThemesSetting,
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<bool> kDarkenWebsitesCheckboxOptOut{
    &kDarkenWebsitesCheckboxInThemesSetting, "opt_out", true};
#endif  // BUILDFLAG(IS_ANDROID)

namespace features {

// Enables unused site permission module in Safety Check.
BASE_FEATURE(kSafetyCheckUnusedSitePermissions,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else   // BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kActiveContentSettingExpiry, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEagerStorageAccessPermissionCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsNoDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-no-delay-for-testing", false};

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsWithDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-with-delay-for-testing", false};

BASE_FEATURE(kApproximateGeolocationPermission,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLeftHandSideActivityIndicators, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLeftHandSideSensorActivityIndicators,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStorageAccessAPIRelatedWebsiteSets,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace content_settings
