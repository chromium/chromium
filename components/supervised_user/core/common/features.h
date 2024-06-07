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
// Only affects Desktop platforms.
BASE_DECLARE_FEATURE(kCustomWebSignInInterceptForSupervisedUsers);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// This hides the following guest mode entry points for supervised users:
//
// * In the Profile menu for supervised profiles
// * In the Profile picker, if there are one or more supervised profiles
BASE_DECLARE_FEATURE(kHideGuestModeForSupervisedUsers);
#endif

// Runs a shadow no-op safe-sites call alongside kids-api call, to compare
// latencies.
BASE_DECLARE_FEATURE(kShadowKidsApiWithSafeSites);

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
#endif

// Returns whether local parent approvals on Family Link user's device are
// enabled.
// Local web approvals are only available when refreshed version of web
// filter interstitial is enabled.
bool IsLocalWebApprovalsEnabled();

// Returns whether the experiment to display a kid-friendly content stream on
// the New Tab page has been enabled.
bool IsKidFriendlyContentFeedAvailable();

// Returns whether to shadow safe-sites call with kids-api call.
bool IsShadowKidsApiWithSafeSitesEnabled();

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
