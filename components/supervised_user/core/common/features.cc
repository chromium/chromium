// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/features.h"
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace supervised_user {

BASE_FEATURE(kKidFriendlyContentFeed,
             "KidFriendlyContentFeed",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kKidFriendlyContentFeedEndpoint{
    &kKidFriendlyContentFeed, "supervised_feed_endpoint", ""};

// Enables local parent approvals for the blocked website on the Family Link
// user's device.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Request priority experiment for ClassifyUrl (for critical path of rendering).
BASE_FEATURE(kHighestRequestPriorityForClassifyUrl,
             "HighestRequestPriorityForClassifyUrl",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGoogleBrandedBuild() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

bool IsLocalWebApprovalsEnabled() {
  // TODO(crbug.com/1272462, b/261729051):
  // Move this logic to SupervisedUserService, once it's migrated to
  // components, and de-release the intended usage of
  // WebsiteParentApproval::IsLocalApprovalSupported for Android.
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kLocalWebApprovals) &&
         IsGoogleBrandedBuild();
#else
  return base::FeatureList::IsEnabled(kLocalWebApprovals);
#endif
}

// The following flags control whether supervision features are enabled on
// desktop and iOS. There are granular sub-feature flags, which control
// particular aspects. If one or more of these sub-feature flags are enabled,
// then child account detection logic is implicitly enabled.
BASE_FEATURE(kFilterWebsitesForSupervisedUsersOnDesktopAndIOS,
             "FilterWebsitesForSupervisedUsersOnDesktopAndIOS",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSupervisedPrefsControlledBySupervisedStore,
             "SupervisedPrefsControlledBySupervisedStore",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Whether to display a "Managed by your parent" or similar text for supervised
// users in various UI surfaces.
BASE_FEATURE(kEnableManagedByParentUi,
             "EnableManagedByParentUi",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kEnableSupervisedUserSkipParentApprovalToInstallExtensions,
             "EnableSupervisedUserSkipParentApprovalToInstallExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kEnableExtensionsPermissionsForSupervisedUsersOnDesktop,
             "EnableExtensionsPermissionsForSupervisedUsersOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Runs a shadow no-op safe-sites call alongside kids-api call, to compare
// latencies.
BASE_FEATURE(kShadowKidsApiWithSafeSites,
             "ShadowKidsApiWithSafeSites",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool CanDisplayFirstTimeInterstitialBanner() {
  return base::FeatureList::IsEnabled(
      kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
}

BASE_FEATURE(kForceGoogleSafeSearchForSupervisedUsers,
             "ForceGoogleSafeSearchForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The URL which the "Managed by your parent" UI links to. This is defined as a
// FeatureParam (but with the currently correct default) because:
// * We expect to change this URL in the near-term, this allows us to gradually
//   roll out that change
// * If the exact URL needs changing this can be done without requiring a binary
//   rollout
constexpr base::FeatureParam<std::string> kManagedByParentUiMoreInfoUrl{
    &kEnableManagedByParentUi, "more_info_url",
    "https://familylink.google.com/setting/resource/94"};

BASE_FEATURE(kCustomWebSignInInterceptForSupervisedUsers,
             "CustomWebSignInInterceptForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMigrateAccountManagementSettingsToCapabilities,
             "MigrateAccountManagementSettingsToCapabilities",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool IsChildAccountSupervisionEnabled() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  // Supervision features are fully supported on Android and ChromeOS.
  return true;
#else
  return base::FeatureList::IsEnabled(
             supervised_user::
                 kFilterWebsitesForSupervisedUsersOnDesktopAndIOS) ||
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
         base::FeatureList::IsEnabled(
             supervised_user::
                 kEnableExtensionsPermissionsForSupervisedUsersOnDesktop) ||
#endif
         base::FeatureList::IsEnabled(
             supervised_user::kSupervisedPrefsControlledBySupervisedStore) ||
         base::FeatureList::IsEnabled(
             supervised_user::kEnableManagedByParentUi);
#endif
}

bool IsKidFriendlyContentFeedAvailable() {
  return base::FeatureList::IsEnabled(kKidFriendlyContentFeed);
}

bool IsShadowKidsApiWithSafeSitesEnabled() {
  return base::FeatureList::IsEnabled(kShadowKidsApiWithSafeSites);
}

}  // namespace supervised_user
