// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/android_buildflags.h"
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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
// Whether we show an error screen in case of failure of a local web approval.
BASE_DECLARE_FEATURE(kEnableLocalWebApprovalErrorDialog);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

// Whether the Pacp widget can process a url payload as part of the local
// approval request.
BASE_DECLARE_FEATURE(kLocalWebApprovalsWidgetSupportsUrlPayload);

// Whether supervised users see an updated URL filter interstitial.
BASE_DECLARE_FEATURE(kSupervisedUserBlockInterstitialV3);

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
#endif

// Fallback to sending un-credentialed filtering requests for supervised users
// if they do not have a valid access token.
BASE_DECLARE_FEATURE(kUncredentialedFilteringFallbackForSupervisedUsers);

// Uses PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable for
// ClassifyUrl fetches.
BASE_DECLARE_FEATURE(kWaitUntilAccessTokenAvailableForClassifyUrl);

// Manages kSupervisedUserSafeSites exclusively within managed user pref store,
// while keeping the default value neutral.
BASE_DECLARE_FEATURE(kAlignSafeSitesValueWithBrowserDefault);

// Allows reading SafeSites setting without extra supervised user guard. Can be
// enabled iff kAlignSafeSitesValueWithBrowserDefault is also enabled.
BASE_DECLARE_FEATURE(kDecoupleSafeSitesFromMainSwitch);

// Returns whether the V3 version of the URL filter interstitial is
// enabled.
bool IsBlockInterstitialV3Enabled();

// Returns whether local parent approvals on Family Link user's device are
// enabled.
bool IsLocalWebApprovalsEnabled();

// Returns whether local parent approvals are enabled for subframe navigation.
bool IsLocalWebApprovalsEnabledForSubframes();

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
