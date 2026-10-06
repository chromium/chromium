// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/common/one_time_token_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace one_time_tokens::features {

// If enabled, Autofill will retrieve one-time passwords from Gmail.
// TODO(crbug.com/452607505): Clean up when launched.
BASE_FEATURE(kGmailOtpRetrievalService,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

const base::FeatureParam<base::TimeDelta> kGmailOtpSubscriptionPeriodParam{
    &kGmailOtpRetrievalService, /*name=*/"subscription_period",
    /*default_value=*/base::Minutes(1)};

const base::FeatureParam<base::TimeDelta>
    kUserDataProcessingConsentFetchTimeoutParam{
        &kGmailOtpRetrievalService,
        /*name=*/"user_data_processing_consent_fetch_timeout",
        /*default_value=*/base::Milliseconds(3000)};

}  // namespace one_time_tokens::features
