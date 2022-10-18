// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_
#define COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace webapps {
namespace features {

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAddToHomescreenMessaging);
BASE_DECLARE_FEATURE(kInstallableAmbientBadgeInfoBar);
BASE_DECLARE_FEATURE(kInstallableAmbientBadgeMessage);
extern const base::FeatureParam<int>
    kInstallableAmbientBadgeMessage_ThrottleDomainsCapacity;
BASE_DECLARE_FEATURE(kWebApkUniqueId);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kCreateShortcutIgnoresManifest);
BASE_DECLARE_FEATURE(kSkipServiceWorkerCheckInstallOnly);
BASE_DECLARE_FEATURE(kDesktopPWAsDetailedInstallDialog);
BASE_DECLARE_FEATURE(kSkipServiceWorkerForInstallPrompt);

bool SkipInstallServiceWorkerCheck();
bool SkipServiceWorkerForInstallPromotion();

}  // namespace features
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_FEATURES_H_
