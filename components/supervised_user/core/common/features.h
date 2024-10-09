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

// Experiment to enable kid-friendly content feed.
BASE_DECLARE_FEATURE(kKidFriendlyContentFeed);

BASE_DECLARE_FEATURE(kLocalWebApprovals);

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

// Enable different web sign in interception behaviour for supervised users:
//
// 1. Supervised user signs in to existing signed out Profile: show modal
//    explaining that supervision features will apply.
// 2. Supervised user signs in as secondary account in existing signed in
//    Profile
//
// Only affects Linux/Mac/Windows platforms.
BASE_DECLARE_FEATURE(kCustomProfileStringsForSupervisedUsers);

// Displays the supervised user signin-in IPH when the child signs
// in to a new or existing local profile.
BASE_DECLARE_FEATURE(kSupervisedUserProfileSigninIPH);

// Displays a Family Link kite badge on the supervised user avatar in various
// surfaces.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kShowKiteForSupervisedUsers);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// This hides the following guest mode entry points for supervised users:
//
// * In the Profile menu for supervised profiles
// * In the Profile picker, if there are one or more supervised profiles
BASE_DECLARE_FEATURE(kHideGuestModeForSupervisedUsers);
#endif

// Force enable SafeSearch for a supervised profile with an
// unauthenticated (e.g. signed out of the content area) account.
BASE_DECLARE_FEATURE(kForceSafeSearchForUnauthenticatedSupervisedUsers);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Force re-authentication when an unauthenticated supervised user tries to
// access YouTube, so that content restrictions can be applied.
BASE_DECLARE_FEATURE(kForceSupervisedUserReauthenticationForYouTube);

// Force re-authentication when an unauthenticated supervised user tries to
// access a blocked site, allowing the user to ask for parent's approval.
BASE_DECLARE_FEATURE(kForceSupervisedUserReauthenticationForBlockedSites);

// Specifies if we should close the sign-in tabs that can be opened from
// the re-authentication interstitial.
BASE_DECLARE_FEATURE(kCloseSignTabsFromReauthenticationInterstitial);

// Shows the subframe re-authentication interstitial for unauthenticated
// supervised users when they try to access:
// * Embedded YouTube videos if re-auth is forced for YouTube.
// * Blocked sites in subframes if re-auth is forced for blocked sites.
//
// This flag is only effective if the flag
// `kForceSupervisedUserReauthenticationForYouTube` or
// `kForceSupervisedUserReauthenticationForBlockedSites` is enabled.
BASE_DECLARE_FEATURE(kAllowSupervisedUserReauthenticationForSubframes);
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

// Updates the ListFamilyMembers service to fetch family account info for
// accounts with the relevant capability rather than just for supervised
// accounts.
BASE_DECLARE_FEATURE(kFetchListFamilyMembersWithCapability);

// Uses `prefs::kFamilyLinkUserMemberRole` to populate the family member role
// for feedback if it is available.
BASE_DECLARE_FEATURE(kUseFamilyMemberRolePrefsForFeedback);

// Alters the behavior of the supervised_user::SupervisedUserNavigationThrottle
// so that the decision whether to proceed or cancel is made when the response
// is ready to be rendered, rather than before the request (or any redirect) is
// issued.
BASE_DECLARE_FEATURE(kClassifyUrlOnProcessResponseEvent);

// Returns whether local parent approvals on Family Link user's device are
// enabled.
// Local web approvals are only available when refreshed version of web
// filter interstitial is enabled.
bool IsLocalWebApprovalsEnabled();

// Returns whether the experiment to display a kid-friendly content stream on
// the New Tab page has been enabled.
bool IsKidFriendlyContentFeedAvailable();

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
