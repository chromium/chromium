// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/features.h"

#include "base/feature_list.h"

namespace webapps {
namespace features {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAddToHomescreenMessaging,
             "AddToHomescreenMessaging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables or disables the installable ambient badge message.
BASE_FEATURE(kInstallPromptGlobalGuardrails,
             "InstallPromptGlobalGuardrails",
             base::FEATURE_ENABLED_BY_DEFAULT);
extern const base::FeatureParam<int>
    kInstallPromptGlobalGuardrails_DismissCount{&kInstallPromptGlobalGuardrails,
                                                "dismiss_count", 3};
extern const base::FeatureParam<base::TimeDelta>
    kInstallPromptGlobalGuardrails_DismissPeriod{
        &kInstallPromptGlobalGuardrails, "dismiss_period", base::Days(7)};
extern const base::FeatureParam<int> kInstallPromptGlobalGuardrails_IgnoreCount{
    &kInstallPromptGlobalGuardrails, "ignore_count", 3};
extern const base::FeatureParam<base::TimeDelta>
    kInstallPromptGlobalGuardrails_IgnorePeriod{&kInstallPromptGlobalGuardrails,
                                                "ignore_period", base::Days(3)};

BASE_FEATURE(kPwaUniversalInstallUi,
             "PwaUniversalInstallUi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables WebAPK Install Failure Notification.
BASE_FEATURE(kWebApkInstallFailureNotification,
             "WebApkInstallFailureNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

// Keys to use when querying the variations params.
BASE_FEATURE(kAppBannerTriggering,
             "AppBannerTriggering",
             base::FEATURE_DISABLED_BY_DEFAULT);
extern const base::FeatureParam<double> kBannerParamsEngagementTotalKey{
    &kAppBannerTriggering, "site_engagement_total",
    kDefaultTotalEngagementToTrigger};
extern const base::FeatureParam<int> kBannerParamsDaysAfterBannerDismissedKey{
    &kAppBannerTriggering, "days_after_dismiss",
    kMinimumBannerBlockedToBannerShown};
extern const base::FeatureParam<int> kBannerParamsDaysAfterBannerIgnoredKey{
    &kAppBannerTriggering, "days_after_ignore", kMinimumDaysBetweenBannerShows};

BASE_FEATURE(kWebAppsEnableMLModelForPromotion,
             "WebAppsEnableMLModelForPromotion",
             base::FEATURE_DISABLED_BY_DEFAULT);
extern const base::FeatureParam<double> kWebAppsMLGuardrailResultReportProb(
    &kWebAppsEnableMLModelForPromotion,
    "guardrail_report_prob",
    0);
extern const base::FeatureParam<double> kWebAppsMLModelUserDeclineReportProb(
    &kWebAppsEnableMLModelForPromotion,
    "model_and_user_decline_report_prob",
    0);
extern const base::FeatureParam<int> kMaxDaysForMLPromotionGuardrailStorage(
    &kWebAppsEnableMLModelForPromotion,
    "max_days_to_store_guardrails",
    kTotalDaysToStoreMLGuardrails);

// Allows installing a web app with fallback manifest values.
BASE_FEATURE(kUniversalInstallManifest,
             "UniversalInstallManifest",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allows installing a web app with fallback manifest values on root scope pages
// without manifest.
BASE_FEATURE(kUniversalInstallRootScopeNoManifest,
             "UniversalInstallRootScopeNoManifest",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allows installing a web app when no icon provided by the manifest.
BASE_FEATURE(kUniversalInstallIcon,
             "UniversalInstallIcon",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

extern const base::FeatureParam<int> kMinimumFaviconSize{&kUniversalInstallIcon,
                                                         "size", 48};

// Allow using default manifest URL.
BASE_FEATURE(kUniversalInstallDefaultUrl,
             "UniversalInstallDefaultUrl",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace webapps
