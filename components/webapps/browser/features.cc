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

BASE_FEATURE(kAmbientBadgeSuppressFirstVisit,
             "AmbientBadgeSuppressFirstVisit",
             base::FEATURE_ENABLED_BY_DEFAULT);

extern const base::FeatureParam<base::TimeDelta>
    kAmbientBadgeSuppressFirstVisit_Period{&kAmbientBadgeSuppressFirstVisit,
                                           "period", base::Days(30)};

// Enables or disables the installable ambient badge infobar.
BASE_FEATURE(kInstallableAmbientBadgeInfoBar,
             "InstallableAmbientBadgeInfoBar",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or disables the installable ambient badge message.
BASE_FEATURE(kInstallableAmbientBadgeMessage,
             "InstallableAmbientBadgeMessage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The capacity of cached domains which do not show message again if
// users do not accept the message.
extern const base::FeatureParam<int>
    kInstallableAmbientBadgeMessage_ThrottleDomainsCapacity{
        &kInstallableAmbientBadgeMessage,
        "installable_ambient_badge_message_throttle_domains_capacity", 100};

// Enables or disables the installable ambient badge message.
BASE_FEATURE(kInstallPromptGlobalGuardrails,
             "InstallPromptGlobalGuardrails",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

// Enables WebAPK Install Failure Notification.
BASE_FEATURE(kWebApkInstallFailureNotification,
             "WebApkInstallFailureNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow user to retry install WebAPK with the failure notification if the
// initial install failed. This needs to be used with
// |kWebApkInstallFailureNotification| Enabled.
BASE_FEATURE(kWebApkInstallFailureRetry,
             "WebApkInstallFailureRetry",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the web app install prompt will be block on the site if
// user ignored the prompt recently. The number of days the prompt will be
// blocked is controlled by feature |kAppBannerTriggering| with params
// |days_after_ignore|.
BASE_FEATURE(kBlockInstallPromptIfIgnoreRecently,
             "BlockInstallPromptIfIgnoreRecently",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows installing a web app with fallback manifest values.
BASE_FEATURE(kUniversalInstallManifest,
             "UniversalInstallManifest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows installing a web app when no icon provided by the manifest.
BASE_FEATURE(kUniversalInstallIcon,
             "UniversalInstallIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

// Use segmentation to decide whether install prompt should be shown.
BASE_FEATURE(kInstallPromptSegmentation,
             "InstallPromptSegmentation",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

}  // namespace features
}  // namespace webapps
