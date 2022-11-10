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

// Enables or disables the installable ambient badge infobar.
BASE_FEATURE(kInstallableAmbientBadgeInfoBar,
             "InstallableAmbientBadgeInfoBar",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the installable ambient badge message.
BASE_FEATURE(kInstallableAmbientBadgeMessage,
             "InstallableAmbientBadgeMessage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The capacity of cached domains which do not show message again if
// users do not accept the message.
extern const base::FeatureParam<int>
    kInstallableAmbientBadgeMessage_ThrottleDomainsCapacity{
        &kInstallableAmbientBadgeMessage,
        "installable_ambient_badge_message_throttle_domains_capacity", 100};

// Enables PWA Unique IDs for WebAPKs.
BASE_FEATURE(kWebApkUniqueId,
             "WebApkUniqueId",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// When the user clicks "Create Shortcut" in the dot menu, the current page is
// used as start-url, instead of the manifest-supplied value.
// This allows subpages of web apps to be bookmarked via shortcuts
// separately from their parent app.
// For installing the parent app, the existing "Install Site" should be used
// instead. With this feature, "Install Site" now also shows up for websites
// without service worker, as long as they have a manifest.
BASE_FEATURE(kCreateShortcutIgnoresManifest,
             "CreateShortcutIgnoresManifest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Skip the service worker install criteria check for installing. This affect
// only the "installable" status but not "promotable".
BASE_FEATURE(kSkipServiceWorkerCheckInstallOnly,
             "SkipServiceWorkerCheckInstallOnly",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables showing a detailed install dialog for user installs.
BASE_FEATURE(kDesktopPWAsDetailedInstallDialog,
             "DesktopPWAsDetailedInstallDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables sending the beforeinstallprompt without a service worker check.
BASE_FEATURE(kSkipServiceWorkerForInstallPrompt,
             "SkipServiceWorkerForInstallPromot",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool SkipInstallServiceWorkerCheck() {
  return base::FeatureList::IsEnabled(kSkipServiceWorkerCheckInstallOnly);
}

bool SkipServiceWorkerForInstallPromotion() {
  return base::FeatureList::IsEnabled(kSkipServiceWorkerCheckInstallOnly) &&
         base::FeatureList::IsEnabled(kSkipServiceWorkerForInstallPrompt);
}

}  // namespace features
}  // namespace webapps
