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
BASE_FEATURE(kFeature,
             "PlusAddressesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kEnterprisePlusAddressOAuthScope{
    &kFeature, kEnterprisePlusAddressOAuthScopeName, ""};
const base::FeatureParam<std::string> kEnterprisePlusAddressServerUrl{
    &kFeature, kEnterprisePlusAddressServerUrlName, ""};
const base::FeatureParam<bool> kSyncWithEnterprisePlusAddressServer{
    &kFeature, kSyncWithEnterprisePlusAddressServerName, false};
const base::FeatureParam<base::TimeDelta> kEnterprisePlusAddressTimerDelay{
    &kFeature, kEnterprisePlusAddressTimerDelayName, base::Hours(24)};
const base::FeatureParam<std::string> kPlusAddressManagementUrl{
    &kFeature, kPlusAddressManagementUrlName, ""};
const base::FeatureParam<std::string> kPlusAddressExcludedSites{
    &kFeature, kPlusAddressExcludedSitesName, ""};
const base::FeatureParam<std::string> kPlusAddressErrorReportUrl{
    &kFeature, kPlusAddressErrorReportUrlName, ""};
const base::FeatureParam<bool> kDisableForForbiddenUsers{
    &kFeature, kDisableForForbiddenUsersName, false};

BASE_FEATURE(kPlusAddressFallbackFromContextMenu,
             "PlusAddressFallbackFromContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, users can refresh the suggested plus address string.
BASE_FEATURE(kPlusAddressRefresh,
             "PlusAddressRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace plus_addresses::features
