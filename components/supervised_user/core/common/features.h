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
// Uses supervised user strings on the signout dialog.
BASE_DECLARE_FEATURE(kEnableSupervisedUserVersionSignOutDialog);
#endif

#if BUILDFLAG(IS_ANDROID)
// The flags below are used to control the local supervision feature on
// Android. To read them, use accessors declared below.
//
// - kPropagateDeviceContentFiltersToSupervisedUser,
// kAllowNonFamilyLinkUrlFilterMode and
// are kSupervisedUserInterstitialWithoutApprovals the three main switches for
// local supervision. They work best when enabled together.
// kPropagateDeviceContentFiltersToSupervisedUser is the main switch that
// enables the feature, kAllowNonFamilyLinkUrlFilterMode gives access to feature
// for signed out users, and kSupervisedUserInterstitialWithoutApprovals enables
// interstitial UI optimized for local supervision.
//
// - kSupervisedUserLocalSupervisionPreview is a convenience feature that
// enables all of the above features if the experimental platform supports them.
// When this feature is enabled and the platform supports local supervision,
// none of the three features are ever read. This allows offering local
// supervision as dogfood feature. Use
// kSupervisedUserLocalSupervisionPreviewBuildVersionMajor parameters to adjust
// the platform build version if needed.
//
// - kSupervisedUserBrowserContentFiltersKillSwitch and
// kSupervisedUserSearchContentFiltersKillSwitch are subswitches of
// kPropagateDeviceContentFiltersToSupervisedUser that control individual
// content filter settings.
// - kSupervisedUserOverrideLocalSupervision is a convenience feature that will
// disable effects of Android Parental Controls if the user is a Family Link
// account. With the flag disabled, the browser strictly expects that at most
// only of of Family Link or Android Parental Controls apply, and terminates the
// browser otherwise.

BASE_DECLARE_FEATURE(kAllowNonFamilyLinkUrlFilterMode);
BASE_DECLARE_FEATURE(kPropagateDeviceContentFiltersToSupervisedUser);
BASE_DECLARE_FEATURE(kSupervisedUserBrowserContentFiltersKillSwitch);
BASE_DECLARE_FEATURE(kSupervisedUserSearchContentFiltersKillSwitch);
BASE_DECLARE_FEATURE(kSupervisedUserInterstitialWithoutApprovals);
BASE_DECLARE_FEATURE(kSupervisedUserLocalSupervisionPreview);
BASE_DECLARE_FEATURE(
    kSupervisedUserOverrideLocalSupervisionForFamilyLinkAccounts);

// The major version of the build that supports local supervision.
extern const base::FeatureParam<std::string>
    kSupervisedUserLocalSupervisionPreviewBuildVersionMajor;

// Returns true when the browser reads the values of content filters for
// local supervision. This is the main gate to local supervision.
bool UseLocalSupervision();
// Indicates if supervised user interstitial should be optimized for local
// supervision, without parent information and approval features.
bool UseInterstitialForLocalSupervision();
// Allows non-signed in users to use url classification feature of local
// supervision.
bool ClassifyUrlWithoutCredentialsForLocalSupervision();
#endif

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
