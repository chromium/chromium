// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/features.h"

namespace private_ai {

BASE_FEATURE(kLegion, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLegionSeverAttestation, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kLegionApiKey{&kLegion, "api-key", ""};

const base::FeatureParam<std::string> kLegionUrl{
    &kLegion, "url",
    "privatearatea-pa.googleapis.com/ws/"
    "mdi.privatearatea.PrivateArateaService.StartNoiseSession"};

const base::FeatureParam<std::string> kLegionProxyServerUrl{&kLegion,
                                                            "proxy-url", ""};

const base::FeatureParam<std::string> kLegionTokenServerUrl{
    &kLegion, /*name=*/"LegionTokenServerUrl",
    /*default_value=*/"https://phosphor-pa.googleapis.com"};

const base::FeatureParam<std::string> kLegionTokenServerGetInitialDataPath{
    &kLegion,
    /*name=*/"LegionTokenServerGetInitialDataPath",
    /*default_value=*/"/v1/privatearateaipp/getInitialData"};

const base::FeatureParam<std::string> kLegionTokenServerGetTokensPath{
    &kLegion, /*name=*/"LegionTokenServerGetTokensPath",
    /*default_value=*/"/v1/privatearateaipp/auth-chrome"};

const base::FeatureParam<int> kLegionAuthTokenCacheBatchSize{
    &kLegion, /*name=*/"LegionAuthTokenCacheBatchSize",
    /*default_value=*/64};

const base::FeatureParam<int> kLegionAuthTokenCacheLowWaterMark{
    &kLegion, /*name=*/"LegionAuthTokenCacheLowWaterMark",
    /*default_value=*/16};

const base::FeatureParam<base::TimeDelta>
    kLegionTryGetAuthTokensNotEligibleBackoff{
        &kLegion,
        /*name=*/"LegionTryGetAuthTokensNotEligibleBackoff",
        /*default_value=*/base::Hours(1)};

const base::FeatureParam<base::TimeDelta>
    kLegionTryGetAuthTokensTransientBackoff{
        &kLegion,
        /*name=*/"LegionTryGetAuthTokensTransientBackoff",
        /*default_value=*/base::Seconds(5)};

const base::FeatureParam<base::TimeDelta> kLegionTryGetAuthTokensBugBackoff{
    &kLegion, /*name=*/"LegionTryGetAuthTokensBugBackoff",
    /*default_value=*/base::Minutes(10)};

const base::FeatureParam<double> kLegionBackoffJitter{
    &kLegion, /*name=*/"LegionBackoffJitter", /*default_value=*/0.25};

}  // namespace private_ai
