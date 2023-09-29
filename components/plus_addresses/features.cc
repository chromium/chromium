// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace plus_addresses {
// Controls the enabled/disabled state of the experimental feature.
BASE_FEATURE(kFeature,
             "PlusAddressesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kEnterprisePlusAddressLabelOverrideName[] = "suggestion-label";
const char kEnterprisePlusAddressOAuthScopeName[] = "oauth-scope";
const char kEnterprisePlusAddressServerUrlName[] = "server-url";
const char kSyncWithEnterprisePlusAddressServerName[] = "sync-with-server";
const char kEnterprisePlusAddressTimerDelayName[] = "timer-delay";
const char kPlusAddressManagementUrlName[] = "manage-url";

const base::FeatureParam<std::string> kEnterprisePlusAddressLabelOverride{
    &kFeature, kEnterprisePlusAddressLabelOverrideName, "Lorem Ipsum"};
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
}  // namespace plus_addresses
