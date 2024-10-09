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

BASE_FEATURE(kEnableSupervisedUserSkipParentApprovalToInstallExtensions,
             "EnableSupervisedUserSkipParentApprovalToInstallExtensions",
#if BUILDFLAG(IS_CHROMEOS)
             // TODO(b/344580484): Cleanup the feature after at least 2
             // milestones from full release, i.e. on M131.
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUpdatedSupervisedUserExtensionApprovalStrings,
             "UpdatedSupervisedUserExtensionApprovalStrings",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kEnableExtensionsPermissionsForSupervisedUsersOnDesktop,
             "EnableExtensionsPermissionsForSupervisedUsersOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
BASE_FEATURE(kExposedParentalControlNeededForExtensionInstallation,
             "ExposedParentalControlNeededForExtensionInstallation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(
      kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  bool skipParentApprovalEnabled = base::FeatureList::IsEnabled(
      kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
  bool permissionExtensionsForSupervisedUsersEnabled =
      base::FeatureList::IsEnabled(
          kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  if (skipParentApprovalEnabled) {
    DCHECK(permissionExtensionsForSupervisedUsersEnabled);
  }
  return skipParentApprovalEnabled &&
         permissionExtensionsForSupervisedUsersEnabled;
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_CHROMEOS)
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

BASE_FEATURE(kCustomProfileStringsForSupervisedUsers,
             "CustomProfileStringsForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSupervisedUserProfileSigninIPH,
             "SupervisedUserProfileSigninIPH",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kShowKiteForSupervisedUsers,
             "ShowKiteForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kHideGuestModeForSupervisedUsers,
             "HideGuestModeForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kForceSafeSearchForUnauthenticatedSupervisedUsers,
             "ForceSafeSearchForUnauthenticatedSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kForceSupervisedUserReauthenticationForYouTube,
             "ForceSupervisedUserReauthenticationForYouTube",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceSupervisedUserReauthenticationForBlockedSites,
             "ForceSupervisedUserReauthenticationForBlockedSites",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCloseSignTabsFromReauthenticationInterstitial,
             "CloseSignTabsFromReauthenticationInterstitial",
             // Enabled by default, flag meant to work as a kill switch.
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowSupervisedUserReauthenticationForSubframes,
             "EnableSupervisedUserReauthenticationForSubframes",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kUncredentialedFilteringFallbackForSupervisedUsers,
             "UncredentialedFilteringFallbackForSupervisedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWaitUntilAccessTokenAvailableForClassifyUrl,
             "WaitUntilAccessTokenAvailableForClassifyUrl",
#if BUILDFLAG(IS_ANDROID)
             // Android enforces at the OS level that supervised users must have
             // valid sign in credentials (and triggers a reauth if not). We can
             // therefore wait for a valid access token to be available before
             // calling ClassifyUrl, to avoid window conditions where the access
             // token is not yet available (eg. during startup).
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // Other platforms don't enforce this, and we therefore cannot
             // wait for access tokens in Chrome.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS,
             "ReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS,
             "ReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kFetchListFamilyMembersWithCapability,
             "FetchListFamilyMembersWithCapability",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUseFamilyMemberRolePrefsForFeedback,
             "UseFamilyMemberRolePrefsForFeedback",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClassifyUrlOnProcessResponseEvent,
             "ClassifyUrlOnProcessResponseEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsKidFriendlyContentFeedAvailable() {
  return base::FeatureList::IsEnabled(kKidFriendlyContentFeed);
}

}  // namespace supervised_user
