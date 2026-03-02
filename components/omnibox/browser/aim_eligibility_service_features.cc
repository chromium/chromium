// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service_features.h"

#include "base/metrics/field_trial_params.h"

namespace omnibox {

BASE_FEATURE(kAimEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilityEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimCoBrowseEligibilityCheckEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimUrlNavigationFetchEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilitySendCoBrowseUserAgentSuffixEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilitySendFullVersionListEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilityCustomRetryPolicyEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilityForPrimaryAccountEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerRequestOnStartupEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimStartupRequestDelayedUntilNetworkAvailableEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerRequestOnIdentityChangeEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<bool> kRequestOnCookieJarChanges{
    &kAimServerRequestOnIdentityChangeEnabled, "request_on_cookie_jar_changes",
    false};
const base::FeatureParam<bool> kRequestOnPrimaryAccountChanges{
    &kAimServerRequestOnIdentityChangeEnabled,
    "request_on_primary_account_changes", true};

BASE_FEATURE(kAimUsePecApi, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAimServerEligibilityIncludeClientLocale,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<AimServerEligibilityIncludeClientLocaleMode>::
    Option kAimServerEligibilityIncludeClientLocaleModeOptions[] = {
        {AimServerEligibilityIncludeClientLocaleMode::kLegacyGet, "legacy_get"},
        {AimServerEligibilityIncludeClientLocaleMode::kGetWithLocale,
         "get_with_locale"},
        {AimServerEligibilityIncludeClientLocaleMode::kPostWithProto,
         "post_with_proto"},
};

const base::FeatureParam<AimServerEligibilityIncludeClientLocaleMode>
    kAimServerEligibilityIncludeClientLocaleMode{
        &kAimServerEligibilityIncludeClientLocale, "mode",
        AimServerEligibilityIncludeClientLocaleMode::kGetWithLocale,
        &kAimServerEligibilityIncludeClientLocaleModeOptions};

BASE_FEATURE(kAimEligibilityServiceIdentityImprovements,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kAimIdentityOauthEnabled{
    &kAimEligibilityServiceIdentityImprovements, "oauth_enabled", true};
const base::FeatureParam<bool> kAimIdentityRefreshOnCookieChanges{
    &kAimEligibilityServiceIdentityImprovements, "refresh_on_cookie_changes",
    true};
const base::FeatureParam<bool> kAimIdentityRefreshOnAccountChanges{
    &kAimEligibilityServiceIdentityImprovements, "refresh_on_account_changes",
    true};
const base::FeatureParam<bool> kAimIdentityDropRequestIfCookiesStale{
    &kAimEligibilityServiceIdentityImprovements,
    "drop_request_if_cookies_stale", true};

BASE_FEATURE(kAimEligibilityServiceDebounce, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kAimEligibilityServiceDebounceDelay{
    &kAimEligibilityServiceDebounce, "delay", base::Milliseconds(100)};

}  // namespace omnibox
