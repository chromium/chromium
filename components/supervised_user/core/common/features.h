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
// Only affects Desktop platforms. This is split into two flags, one controlling
// the infrastructure and one controlling the UI changes.

// Waits for the async signal that a user is supervised.
BASE_DECLARE_FEATURE(kCustomWebSignInInterceptForSupervisedUsers);

// Displays custom UI based on the async signal above. Only used if
// kCustomWebSignInInterceptForSupervisedUsers is enabled.
BASE_DECLARE_FEATURE(kCustomWebSignInInterceptForSupervisedUsersUi);

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

// Fallback to sending un-credentialed filtering requests for supervised users
// if they do not have a valid access token.
BASE_DECLARE_FEATURE(kUncredentialedFilteringFallbackForSupervisedUsers);

// Updates usages of Profile.isChild() in Profile.java to use the account
// capability to determine if account is supervised.
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kMigrateAccountManagementSettingsToCapabilities);
#endif

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
