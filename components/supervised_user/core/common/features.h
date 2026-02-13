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

// SupervisedUserUrlFilteringService does not use the PrefService indirection
// (specifically, the SupervisedUserPrefStore) to get the URL filtering
// settings. When enabled, all url filtering settings are read directly from the
// related supervision services.
BASE_DECLARE_FEATURE(kSupervisedUserUseUrlFilteringService);

// The SupervisedUserPrefStore will merge all of the non-web filtering device
// parental controls settings with the Family Link settings and emit merged
// values as prefs.
BASE_DECLARE_FEATURE(
    kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs);

// When enabled, the supervised user log record will emit the device
// log record separately. When disabled, the system assumes that the device log
// record is mutually exclusive with the account/policy based log record.
BASE_DECLARE_FEATURE(kSupervisedUserEmitLogRecordSeparately);

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
