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
constexpr unsigned int kMinimumDaysBetweenBannerShows = 7;

// Default site engagement required to trigger the banner.
constexpr unsigned int kDefaultTotalEngagementToTrigger = 2;

// Default amount of days after which the guardrail information about user
// cancellations and dismissals on the ML promoted installation dialog is
// automatically cleared. To understand more on how this works, please refer to
// `kMlPromoGuardrails` in web_app_pref_guardrails.h.
inline constexpr int kTotalDaysToStoreMLGuardrails = 180;

// Min icon size when using favicon to install webapp.
inline constexpr int kMinimumFaviconSize = 48;

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAddToHomescreenMessaging);

BASE_DECLARE_FEATURE(kWebApkInstallFailureNotification);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kAppBannerTriggering);
extern const base::FeatureParam<double> kBannerParamsEngagementTotalKey;
extern const base::FeatureParam<int> kBannerParamsDaysAfterBannerDismissedKey;
extern const base::FeatureParam<int> kBannerParamsDaysAfterBannerIgnoredKey;

// ML Installability promotion flags and all the feature params.
BASE_DECLARE_FEATURE(kWebAppsEnableMLModelForPromotion);
extern const base::FeatureParam<double> kWebAppsMLGuardrailResultReportProb;
extern const base::FeatureParam<double> kWebAppsMLModelUserDeclineReportProb;
extern const base::FeatureParam<int> kMaxDaysForMLPromotionGuardrailStorage;

BASE_DECLARE_FEATURE(kBypassAppBannerEngagementChecks);

}  // namespace features
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_
