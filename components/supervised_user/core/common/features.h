// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace supervised_user {

// Experiment to enable kid-friendly content feed.
BASE_DECLARE_FEATURE(kKidFriendlyContentFeed);
extern const base::FeatureParam<std::string> kKidFriendlyContentFeedEndpoint;

BASE_DECLARE_FEATURE(kLocalWebApprovals);

// Flags related to supervision features on Desktop and iOS platforms.
BASE_DECLARE_FEATURE(kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
BASE_DECLARE_FEATURE(kSupervisedPrefsControlledBySupervisedStore);
BASE_DECLARE_FEATURE(kEnableManagedByParentUi);
extern const base::FeatureParam<std::string> kManagedByParentUiMoreInfoUrl;
BASE_DECLARE_FEATURE(kClearingCookiesKeepsSupervisedUsersSignedIn);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif

// Returns whether banner can be displayed to the user after website filtering
// is enabled
bool CanDisplayFirstTimeInterstitialBanner();

// Experiments to enable proto fetchers
BASE_DECLARE_FEATURE(kEnableProtoApiForClassifyUrl);

// Instead of manually implementing the process, use the proto_fetcher.cc's one.
BASE_DECLARE_FEATURE(kUseBuiltInRetryingMechanismForListFamilyMembers);

// Enable different web sign in interception behaviour for supervised users:
//
// 1. Supervised user signs in to existing signed out Profile: show modal
//    explaining that supervision features will apply.
// 2. Supervised user signs in as secondary account in existing signed in
//    Profile
//
// Only affects Desktop platforms.
BASE_DECLARE_FEATURE(kCustomWebSignInInterceptForSupervisedUsers);

// Runs a shadow no-op safe-sites call alongside kids-api call, to compare
// latencies.
BASE_DECLARE_FEATURE(kShadowKidsApiWithSafeSites);

// Forces Safe Search for supervised users.
BASE_DECLARE_FEATURE(kForceGoogleSafeSearchForSupervisedUsers);

// Returns whether local parent approvals on Family Link user's device are
// enabled.
// Local web approvals are only available when refreshed version of web
// filter interstitial is enabled.
bool IsLocalWebApprovalsEnabled();

// Returns whether the ClassifyUrl call uses proto apis.
bool IsProtoApiForClassifyUrlEnabled();

// Decides whether to use built-in configurable mechanism, instead of manually
// programmed.
bool IsRetryMechanismForListFamilyMembersEnabled();

// Returns true if child account supervision features should be enabled for this
// client.
//
// This method does not take into account whether the user is actually a child;
// that must be handled by calling code.
bool IsChildAccountSupervisionEnabled();

// Returns whether the experiment to display a kid-friendly content stream on
// the New Tab page has been enabled.
bool IsKidFriendlyContentFeedAvailable();

// Returns whether to shadow safe-sites call with kids-api call.
bool IsShadowKidsApiWithSafeSitesEnabled();

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_FEATURES_H_
