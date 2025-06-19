// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/features.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace supervised_user {

// Enables local parent approvals for the blocked website on the Family Link
// user's device.
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// TODO(crbug.com/391799078): Support local web approval for subframes on
// Desktop.
BASE_FEATURE(kAllowSubframeLocalWebApprovals,
             "AllowSubframeLocalWebApprovals",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_WIN)
const int kLocalWebApprovalBottomSheetLoadTimeoutDefaultValueMs = 5000;

const base::FeatureParam<int> kLocalWebApprovalBottomSheetLoadTimeoutMs{
    &kLocalWebApprovals, /*name=*/"LocalWebApprovalBottomSheetLoadTimeoutMs",
    kLocalWebApprovalBottomSheetLoadTimeoutDefaultValueMs};
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kEnableLocalWebApprovalErrorDialog,
             "EnableLocalWebApprovalErrorDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

BASE_FEATURE(kLocalWebApprovalsWidgetSupportsUrlPayload,
             "PacpWidgetSupportsUrlPayload",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSupervisedUserBlockInterstitialV3,
             "SupervisedUserBlockInterstitialV3",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGoogleBrandedBuild() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

bool IsBlockInterstitialV3Enabled() {
  return base::FeatureList::IsEnabled(kSupervisedUserBlockInterstitialV3);
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

bool IsLocalWebApprovalsEnabledForSubframes() {
  return base::FeatureList::IsEnabled(kAllowSubframeLocalWebApprovals);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kCustomProfileStringsForSupervisedUsers,
             "CustomProfileStringsForSupervisedUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShowKiteForSupervisedUsers,
             "ShowKiteForSupervisedUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kForceSafeSearchForUnauthenticatedSupervisedUsers,
             "ForceSafeSearchForUnauthenticatedSupervisedUsers",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kEnableSupervisedUserVersionSignOutDialog,
             "EnableSupervisedUserVersionSignOutDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif


BASE_FEATURE(kAlignSafeSitesValueWithBrowserDefault,
             "AlignSafeSitesValueWithBrowserDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDecoupleSafeSitesFromMainSwitch,
             "DecoupleSafeSitesFromMainSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAllowNonFamilyLinkUrlFilterMode,
             "AllowNonFamilyLinkUrlFilterMode",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPropagateDeviceContentFiltersToSupervisedUser,
             "PropagateDeviceContentFiltersToSupervisedUser",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSupervisedUserInterstitialWithoutApprovals,
             "SupervisedUserInterstitialWithoutApprovals",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace supervised_user
