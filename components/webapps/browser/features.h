// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_
#define COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace webapps {
namespace features {

// Default number of days that dismissing or ignoring the banner will prevent it
// being seen again for.
constexpr unsigned int kMinimumBannerBlockedToBannerShown = 90;
constexpr unsigned int kMinimumDaysBetweenBannerShows = 14;

// Default site engagement required to trigger the banner.
constexpr unsigned int kDefaultTotalEngagementToTrigger = 2;

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAddToHomescreenMessaging);
BASE_DECLARE_FEATURE(kAmbientBadgeSuppressFirstVisit);
extern const base::FeatureParam<base::TimeDelta>
    kAmbientBadgeSuppressFirstVisit_Period;
BASE_DECLARE_FEATURE(kInstallableAmbientBadgeInfoBar);
BASE_DECLARE_FEATURE(kInstallableAmbientBadgeMessage);
extern const base::FeatureParam<int>
    kInstallableAmbientBadgeMessage_ThrottleDomainsCapacity;
BASE_DECLARE_FEATURE(kInstallPromptGlobalGuardrails);
extern const base::FeatureParam<int>
    kInstallPromptGlobalGuardrails_DismissCount;
extern const base::FeatureParam<base::TimeDelta>
    kInstallPromptGlobalGuardrails_DismissPeriod;
extern const base::FeatureParam<int> kInstallPromptGlobalGuardrails_IgnoreCount;
extern const base::FeatureParam<base::TimeDelta>
    kInstallPromptGlobalGuardrails_IgnorePeriod;
BASE_DECLARE_FEATURE(kWebApkInstallFailureNotification);
BASE_DECLARE_FEATURE(kWebApkInstallFailureRetry);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kCreateShortcutIgnoresManifest);
BASE_DECLARE_FEATURE(kSkipServiceWorkerCheckInstallOnly);
BASE_DECLARE_FEATURE(kDesktopPWAsDetailedInstallDialog);
BASE_DECLARE_FEATURE(kSkipServiceWorkerForInstallPrompt);

BASE_DECLARE_FEATURE(kInstallPromptSegmentation);

bool SkipInstallServiceWorkerCheck();
bool SkipServiceWorkerForInstallPromotion();

BASE_DECLARE_FEATURE(kAppBannerTriggering);
extern const base::FeatureParam<double> kBannerParamsEngagementTotalKey;
extern const base::FeatureParam<int> kBannerParamsDaysAfterBannerDismissedKey;
extern const base::FeatureParam<int> kBannerParamsDaysAfterBannerIgnoredKey;

BASE_DECLARE_FEATURE(kWebAppsMlUkmCollection);

}  // namespace features
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_
