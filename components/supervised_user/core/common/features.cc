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

namespace supervised_user {

// Enables refreshed version of the website filter interstitial that is shown to
// Family Link users when they navigate to the blocked website.
// This feature is a prerequisite for `kLocalWebApproval` feature.
//
// TODO(b/276428131): clean up this feature once local approvals on Android is
// fully launched.
BASE_FEATURE(kWebFilterInterstitialRefresh,
             "WebFilterInterstitialRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables local parent approvals for the blocked website on the Family Link
// user's device.
// This feature requires a refreshed layout and `kWebFilterInterstitialRefresh`
// to be enabled.
//
// The feature includes one experiment parameter: "preferred_button", which
// determines which button is displayed as the preferred option in the
// interstitial UI (i.e. dark blue button).
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

const char kLocalWebApprovalsPreferredButtonLocal[] = "local";
const char kLocalWebApprovalsPreferredButtonRemote[] = "remote";
constexpr base::FeatureParam<std::string> kLocalWebApprovalsPreferredButton{
    &kLocalWebApprovals, "preferred_button",
    kLocalWebApprovalsPreferredButtonLocal};

// Enables the proto api for ClassifyURL calls.
BASE_FEATURE(kEnableProtoApiForClassifyUrl,
             "EnableProtoApiForClassifyUrl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables synchronous sign-in checking in the First Run Experience.
BASE_FEATURE(kSynchronousSignInChecking,
             "SynchronousSignInChecking",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Retires the static denylist functionality - this serves as a kill-switch.
BASE_FEATURE(kRetireStaticDenyList,
             "RetireStaticDenyList",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the new local extension approvals experience, which requests approval
// through a platform-specific Parent Access Widget. Available on ChromeOS.
BASE_FEATURE(kLocalExtensionApprovalsV2,
             "LocalExtensionApprovalsV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Stops creating Supervised User Service for Incognito profile.
BASE_FEATURE(kUpdateSupervisedUserFactoryCreation,
             "UpdateSupervisedUserFactoryCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsWebFilterInterstitialRefreshEnabled() {
  CHECK(base::FeatureList::IsEnabled(kWebFilterInterstitialRefresh) ||
        !base::FeatureList::IsEnabled(kLocalWebApprovals));
  return base::FeatureList::IsEnabled(kWebFilterInterstitialRefresh);
}

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
  // WebsiteParentApproval::IsLocalApprovalSupported for Andoird.
#if BUILDFLAG(IS_ANDROID)
  return IsWebFilterInterstitialRefreshEnabled() &&
         base::FeatureList::IsEnabled(kLocalWebApprovals) &&
         IsGoogleBrandedBuild();
#else
  return IsWebFilterInterstitialRefreshEnabled() &&
         base::FeatureList::IsEnabled(kLocalWebApprovals);
#endif
}

bool IsLocalWebApprovalThePreferredButton() {
  std::string preferred_button = kLocalWebApprovalsPreferredButton.Get();
  DCHECK((preferred_button == kLocalWebApprovalsPreferredButtonLocal) ||
         (preferred_button == kLocalWebApprovalsPreferredButtonRemote));
  return (preferred_button == kLocalWebApprovalsPreferredButtonLocal);
}

bool IsProtoApiForClassifyUrlEnabled() {
  return base::FeatureList::IsEnabled(kEnableProtoApiForClassifyUrl);
}

// The following flags control whether supervision features are enabled on
// desktop and iOS. These are structured as follows:
//
// * EnableSupervisionOnDesktopAndIOS controls whether *any* supervision
// features are enabled at all.
// * Individual granular per-feature flags that control whether individual
// features are enabled. These should only be enabled if
// EnableSupervisionOnDesktopAndIOS is also enabled.
//
// For a feature to be enabled:
// * EnableSupervisionOnDesktopAndIOS must be enabled
// * If that feature has a granular feature flag, it must also be enabled
BASE_FEATURE(kEnableSupervisionOnDesktopAndIOS,
             "EnableSupervisionOnDesktopAndIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFilterWebsitesForSupervisedUsersOnDesktopAndIOS,
             "FilterWebsitesForSupervisedUsersOnDesktopAndIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableExtensionsPermissionsForSupervisedUsersOnDesktop,
             "EnableExtensionsPermissionsForSupervisedUsersOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSupervisedPrefsControlledBySupervisedStore,
             "SupervisedPrefsControlledBySupervisedStore",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableCreatePermissionRequestFetcher,
             "EnableCreatePermissionRequestFetcher",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to display a "Managed by your parent" or similar text for supervised
// users in various UI surfaces.
BASE_FEATURE(kEnableManagedByParentUi,
             "EnableManagedByParentUi",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool CanDisplayFirstTimeInterstitialBanner() {
  return base::FeatureList::IsEnabled(kEnableSupervisionOnDesktopAndIOS) &&
         base::FeatureList::IsEnabled(
             kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
}

// When enabled non-syncing signed in supervised users will not be signed out of
// their google account when cookies are cleared
BASE_FEATURE(kClearingCookiesKeepsSupervisedUsersSignedIn,
             "ClearingCookiesKeepsSupervisedUsersSignedIn",
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

bool IsSynchronousSignInCheckingEnabled() {
  return base::FeatureList::IsEnabled(kSynchronousSignInChecking);
}

bool IsLocalExtensionApprovalsV2Enabled() {
  return base::FeatureList::IsEnabled(kLocalExtensionApprovalsV2);
}
}  // namespace supervised_user
