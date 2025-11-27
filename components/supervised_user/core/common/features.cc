// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/features.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace supervised_user {

// Enables local parent approvals for the blocked website on the Family Link
// user's device.
BASE_FEATURE(kLocalWebApprovals, base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/391799078): Support local web approval for subframes on
// Desktop.
BASE_FEATURE(kAllowSubframeLocalWebApprovals,
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
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

BASE_FEATURE(kLocalWebApprovalsWidgetSupportsUrlPayload,
             "PacpWidgetSupportsUrlPayload",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/435635774): Release the interstitial v3 in all platforms.
BASE_FEATURE(kSupervisedUserBlockInterstitialV3,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_IOS)

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
BASE_FEATURE(kEnableSupervisedUserVersionSignOutDialog,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAllowNonFamilyLinkUrlFilterMode,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPropagateDeviceContentFiltersToSupervisedUser,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSupervisedUserBrowserContentFiltersKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSupervisedUserSearchContentFiltersKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSupervisedUserInterstitialWithoutApprovals,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSupervisedUserLocalSupervisionPreview,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSupervisedUserOverrideLocalSupervisionForFamilyLinkAccounts,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string>
    kSupervisedUserLocalSupervisionPreviewBuildVersionMajor{
        &kSupervisedUserLocalSupervisionPreview, "build_version_major", "BP41"};

namespace {
bool PlatformSupportsLocalSupervision() {
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  bool supported_by_os = (major == 16 && minor >= 1) || major > 16;

  bool supported_by_build = base::StartsWith(
      base::SysInfo::GetAndroidBuildID(),
      kSupervisedUserLocalSupervisionPreviewBuildVersionMajor.Get(),
      base::CompareCase::SENSITIVE);

  return supported_by_os || supported_by_build;
}

bool IsLocalSupervisionEnabled() {
  return PlatformSupportsLocalSupervision() &&
         base::FeatureList::IsEnabled(kSupervisedUserLocalSupervisionPreview);
}
}  // namespace

bool UseLocalSupervision() {
  return IsLocalSupervisionEnabled() ||
         base::FeatureList::IsEnabled(
             kPropagateDeviceContentFiltersToSupervisedUser);
}
bool UseInterstitialForLocalSupervision() {
  return IsLocalSupervisionEnabled() ||
         base::FeatureList::IsEnabled(
             kSupervisedUserInterstitialWithoutApprovals);
}
bool ClassifyUrlWithoutCredentialsForLocalSupervision() {
  return IsLocalSupervisionEnabled() ||
         base::FeatureList::IsEnabled(kAllowNonFamilyLinkUrlFilterMode);
}

#endif

}  // namespace supervised_user
