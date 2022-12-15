// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/features.h"
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace supervised_users {

// Enables refreshed version of the website filter interstitial that is shown to
// Family Link users when the navigate to the blocked website.
// This feature is a prerequisite for `kLocalWebApproval` feature.
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kWebFilterInterstitialRefresh,
             "WebFilterInterstitialRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kWebFilterInterstitialRefresh,
             "WebFilterInterstitialRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Enables local parent approvals for the blocked website on the Family Link
// user's device.
// This feature requires a refreshed layout and `kWebFilterInterstitialRefresh`
// to be enabled.
//
// The feature includes one experiment parameter: "preferred_button", which
// determines which button is displayed as the preferred option in the
// interstitial UI (i.e. dark blue button).
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kLocalWebApprovals,
             "LocalWebApprovals",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

const char kLocalWebApprovalsPreferredButtonLocal[] = "local";
const char kLocalWebApprovalsPreferredButtonRemote[] = "remote";
constexpr base::FeatureParam<std::string> kLocalWebApprovalsPreferredButton{
    &kLocalWebApprovals, "preferred_button",
    kLocalWebApprovalsPreferredButtonLocal};

// Enables child accounts (i.e. Unicorn accounts) to clear their browsing
// history data from Settings.
#if BUILDFLAG(IS_CHROMEOS)
// TODO(b/251192695): launch on Chrome OS
BASE_FEATURE(kAllowHistoryDeletionForChildAccounts,
             "AllowHistoryDeletionForChildAccounts",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kAllowHistoryDeletionForChildAccounts,
             "AllowHistoryDeletionForChildAccounts",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Enables the new Kids Management Api.
BASE_FEATURE(kEnableKidsManagementService,
             "EnableKidsManagementService",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsWebFilterInterstitialRefreshEnabled() {
  DCHECK(base::FeatureList::IsEnabled(kWebFilterInterstitialRefresh) ||
         !base::FeatureList::IsEnabled(kLocalWebApprovals));
  return base::FeatureList::IsEnabled(kWebFilterInterstitialRefresh);
}

bool IsLocalWebApprovalsEnabled() {
  // TODO(crbug.com/1272462): on Android also call through to Java code to check
  // whether the feature is supported.
  return IsWebFilterInterstitialRefreshEnabled() &&
         base::FeatureList::IsEnabled(kLocalWebApprovals);
}

bool IsLocalWebApprovalThePreferredButton() {
  std::string preferred_button = kLocalWebApprovalsPreferredButton.Get();
  DCHECK((preferred_button == kLocalWebApprovalsPreferredButtonLocal) ||
         (preferred_button == kLocalWebApprovalsPreferredButtonRemote));
  return (preferred_button == kLocalWebApprovalsPreferredButtonLocal);
}

bool IsKidsManagementServiceEnabled() {
  return base::FeatureList::IsEnabled(kEnableKidsManagementService);
}

}  // namespace supervised_users
