// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content_settings {

// Enables an improved UI for third-party cookie blocking in incognito mode.
#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kImprovedCookieControls,
             "ImprovedCookieControls",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

// Enables auto dark feature in theme settings.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kDarkenWebsitesCheckboxInThemesSetting,
             "DarkenWebsitesCheckboxInThemesSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<bool> kDarkenWebsitesCheckboxOptOut{
    &kDarkenWebsitesCheckboxInThemesSetting, "opt_out", true};
#endif  // BUILDFLAG(IS_ANDROID)

namespace features {

// Enables unused site permission module in Safety Check.
BASE_FEATURE(kSafetyCheckUnusedSitePermissions,
             "SafetyCheckUnusedSitePermissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRepeatedUpdateInterval{
        &kSafetyCheckUnusedSitePermissions,
        "unused-site-repeated-update-interval", base::Days(1)};

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsNoDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-no-delay-for-testing", false};

const base::FeatureParam<bool> kSafetyCheckUnusedSitePermissionsWithDelay{
    &kSafetyCheckUnusedSitePermissions,
    "unused-site-permissions-with-delay-for-testing", false};

const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRevocationThreshold{
        &kSafetyCheckUnusedSitePermissions,
        "unused-site-permissions-revocation-threshold", base::Days(60)};

const base::FeatureParam<base::TimeDelta>
    kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold{
        &kSafetyCheckUnusedSitePermissions,
        "unused-site-permissions-revocation-cleanup-threshold", base::Days(30)};

}  // namespace features
}  // namespace content_settings
