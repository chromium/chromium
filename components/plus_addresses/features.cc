// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace plus_addresses::features {

namespace {

constexpr char kEnterprisePlusAddressOAuthScopeName[] = "oauth-scope";
constexpr char kEnterprisePlusAddressServerUrlName[] = "server-url";
constexpr char kSyncWithEnterprisePlusAddressServerName[] = "sync-with-server";
constexpr char kEnterprisePlusAddressTimerDelayName[] = "timer-delay";
constexpr char kPlusAddressManagementUrlName[] = "manage-url";
constexpr char kPlusAddressExcludedSitesName[] = "excluded-sites";
constexpr char kPlusAddressErrorReportUrlName[] = "error-report-url";
constexpr char kDisableForForbiddenUsersName[] = "disable-for-forbidden-users";

}  // namespace

// Controls the enabled/disabled state of the experimental feature.
BASE_FEATURE(kPlusAddressesEnabled,
             "PlusAddressesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kEnterprisePlusAddressOAuthScope{
    &kPlusAddressesEnabled, kEnterprisePlusAddressOAuthScopeName, ""};
const base::FeatureParam<std::string> kEnterprisePlusAddressServerUrl{
    &kPlusAddressesEnabled, kEnterprisePlusAddressServerUrlName, ""};
const base::FeatureParam<bool> kSyncWithEnterprisePlusAddressServer{
    &kPlusAddressesEnabled, kSyncWithEnterprisePlusAddressServerName, false};
const base::FeatureParam<base::TimeDelta> kEnterprisePlusAddressTimerDelay{
    &kPlusAddressesEnabled, kEnterprisePlusAddressTimerDelayName,
    base::Hours(24)};
const base::FeatureParam<std::string> kPlusAddressManagementUrl{
    &kPlusAddressesEnabled, kPlusAddressManagementUrlName, ""};
const base::FeatureParam<std::string> kPlusAddressExcludedSites{
    &kPlusAddressesEnabled, kPlusAddressExcludedSitesName, ""};
const base::FeatureParam<std::string> kPlusAddressErrorReportUrl{
    &kPlusAddressesEnabled, kPlusAddressErrorReportUrlName, ""};
const base::FeatureParam<bool> kDisableForForbiddenUsers{
    &kPlusAddressesEnabled, kDisableForForbiddenUsersName, false};

// When enabled, plus addresses are supported within the context menu.
BASE_FEATURE(kPlusAddressFallbackFromContextMenu,
             "PlusAddressFallbackFromContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, plus address refresh requests to the backend are supported.
BASE_FEATURE(kPlusAddressRefresh,
             "PlusAddressRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, refresh UI is shown in the modal creation dialog on Desktop.
BASE_FEATURE(kPlusAddressRefreshUiInDesktopModal,
             "PlusAddressRefreshUiInDesktopModal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, refresh UI is shown in the bottom sheet on iOS.
BASE_FEATURE(kPlusAddressRefreshUiInIOS,
             "PlusAddressRefreshUiInIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allows the use of affiliation data with plus addresses. This
// includes things like prefetching affiliation data, or suggesting plus
// addresses for affiliated domains.
BASE_FEATURE(kPlusAddressAffiliations,
             "PlusAddressAffiliations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the redesigned UI for plus addresses on all platforms.
BASE_FEATURE(kPlusAddressUIRedesign,
             "PlusAddressUIRedesign",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, refresh UI is shown in the bottom sheet on Android.
BASE_FEATURE(kPlusAddressRefreshUiInAndroid,
             "PlusAddressRefreshUiInAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace plus_addresses::features
