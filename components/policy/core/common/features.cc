// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/features.h"

#include "google_apis/gaia/gaia_constants.h"

namespace policy {

namespace features {

BASE_FEATURE(kDefaultChromeAppsMigration,
             "EnableDefaultAppsMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLoginEventReporting,
             "LoginEventReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordBreachEventReporting,
             "PasswordBreachEventReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableUserCloudSigninRestrictionPolicyFetcher,
             "UserCloudSigninRestrictionPolicyFetcher",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kActivateMetricsReportingEnabledPolicyAndroid,
             "ActivateMetricsReportingEnabledPolicyAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDmTokenDeletion,
             "DmTokenDeletion",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

}  // namespace policy
