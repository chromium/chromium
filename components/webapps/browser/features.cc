// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/features.h"

#include "base/feature_list.h"

namespace webapps {
namespace features {

#if BUILDFLAG(IS_ANDROID)
// Enables WebAPK Install Failure Notification.
BASE_FEATURE(kWebApkInstallFailureNotification,
             "WebApkInstallFailureNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidMinimalUiLargeScreen,
             "AndroidMinimalUiLargeScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Do not remove this feature flag, since it serves as a kill-switch for the ML
// promotion model. Kill switches are required for all ML model-backed features.
BASE_FEATURE(kWebAppsEnableMLModelForPromotion,
             "WebAppsEnableMLModelForPromotion",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)
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

// Checking if a web app is installed in Chrome Android ultimately leads to a
// long, UI-thread Binder call. Enabling this flag makes the web app
// installation check on Clank async.
BASE_FEATURE(kCheckWebAppExistenceAsync,
             "CheckWebAppExistenceAsync",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features
}  // namespace webapps
