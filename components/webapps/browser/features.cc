// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/features.h"

#include "base/feature_list.h"

namespace webapps {
namespace features {

#if BUILDFLAG(IS_ANDROID)
const base::Feature kAddToHomescreenMessaging{
    "AddToHomescreenMessaging", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the installable ambient badge infobar.
const base::Feature kInstallableAmbientBadgeInfoBar{
    "InstallableAmbientBadgeInfoBar", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the installable ambient badge message.
const base::Feature kInstallableAmbientBadgeMessage{
    "InstallableAmbientBadgeMessage", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_ANDROID)

// Skip the service worker all install criteria check.
const base::Feature kSkipServiceWorkerCheckAll{
    "SkipServiceWorkerCheckAll", base::FEATURE_DISABLED_BY_DEFAULT};

// Skip the service worker install criteria check for installing WebAPKs.
const base::Feature kSkipServiceWorkerCheckInstallOnly{
    "SkipServiceWorkerCheckInstallOnly", base::FEATURE_DISABLED_BY_DEFAULT};

bool SkipBannerServiceWorkerCheck() {
  return base::FeatureList::IsEnabled(kSkipServiceWorkerCheckAll);
}

bool SkipInstallServiceWorkerCheck() {
  return base::FeatureList::IsEnabled(kSkipServiceWorkerCheckAll) ||
         base::FeatureList::IsEnabled(kSkipServiceWorkerCheckInstallOnly);
}

}  // namespace features
}  // namespace webapps
