// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

namespace supervised_user {

BASE_DECLARE_FEATURE(kLocalWebApprovals);

// Whether supervised user can request local web approval from a blocked
// subframe.
BASE_DECLARE_FEATURE(kAllowSubframeLocalWebApprovals);

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_WIN)
extern const base::FeatureParam<int> kLocalWebApprovalBottomSheetLoadTimeoutMs;
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_WIN)

// Whether the Pacp widget can process a url payload as part of the local
// approval request.
BASE_DECLARE_FEATURE(kLocalWebApprovalsWidgetSupportsUrlPayload);

// Applies the updated extension approval flow, which can skip parent-approvals
// on extension installations.
BASE_DECLARE_FEATURE(
    kEnableSupervisedUserSkipParentApprovalToInstallExtensions);

// Applies new informative strings during the parental extension approval flow.
BASE_DECLARE_FEATURE(kUpdatedSupervisedUserExtensionApprovalStrings);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Returns whether a new installation state for supervised users
// on new extension installations is offered to the Webstore.
BASE_DECLARE_FEATURE(kExposedParentalControlNeededForExtensionInstallation);

// Returns whether the new mode for extension approval management is enabled.
// Under this mode, supervised users may request parent approval on each
// extension installation or the parent allows and approves by default all
// extension installations.
// On Win/Linux/Mac enabling the new mode requires that the feature
// `kEnableExtensionsPermissionsForSupervisedUsersOnDesktop` is also enabled.
bool IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Enable different web sign in interception behaviour for supervised users:
//
// 1. Supervised user signs in to existing signed out Profile: show modal
//    explaining that supervision features will apply.
// 2. Supervised user signs in as secondary account in existing signed in
//    Profile
BASE_DECLARE_FEATURE(kCustomProfileStringsForSupervisedUsers);

// Displays a Family Link kite badge on the supervised user avatar in various
// surfaces.
BASE_DECLARE_FEATURE(kShowKiteForSupervisedUsers);
#endif

// Force enable SafeSearch for a supervised profile with an
// unauthenticated (e.g. signed out of the content area) account.
BASE_DECLARE_FEATURE(kForceSafeSearchForUnauthenticatedSupervisedUsers);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Uses supervised user strings on the signout dialog.
BASE_DECLARE_FEATURE(kEnableSupervisedUserVersionSignOutDialog);

// Force re-authentication when an unauthenticated supervised user tries to
// access YouTube, so that content restrictions can be applied.
BASE_DECLARE_FEATURE(kForceSupervisedUserReauthenticationForYouTube);

// Specifies if infrastructure-related YouTube endpoints should be still
// reachable even if parental controls related restrict YouTube access.
BASE_DECLARE_FEATURE(kExemptYouTubeInfrastructureFromBlocking);
#endif

// Fallback to sending un-credentialed filtering requests for supervised users
// if they do not have a valid access token.
BASE_DECLARE_FEATURE(kUncredentialedFilteringFallbackForSupervisedUsers);

// Uses PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable for
// ClassifyUrl fetches.
BASE_DECLARE_FEATURE(kWaitUntilAccessTokenAvailableForClassifyUrl);

#if BUILDFLAG(IS_IOS)
// Replaces usages of prefs::kSupervisedUserID with AccountInfo capabilities on
// iOS.
BASE_DECLARE_FEATURE(kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS);
// Replaces usages of system capabilities with AccountInfo capabilities on iOS.
BASE_DECLARE_FEATURE(
    kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS);
#endif

// Returns whether local parent approvals on Family Link user's device are
// enabled.
// Local web approvals are only available when refreshed version of web
// filter interstitial is enabled.
bool IsLocalWebApprovalsEnabled();

// Returns whether local parent approvals are enabled for subframe navigation.
bool IsLocalWebApprovalsEnabledForSubframes();

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
