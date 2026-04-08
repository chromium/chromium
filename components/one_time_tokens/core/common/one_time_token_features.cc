// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/common/one_time_token_features.h"

#include "base/feature_list.h"

namespace one_time_tokens::features {

// If enabled, Autofill will retrieve one-time passwords from Gmail.
// TODO(crbug.com/452607505): Clean up when launched.
BASE_FEATURE(kGmailOtpRetrievalService, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kOneTimeTokenServiceUrl{
    &kGmailOtpRetrievalService, /*name=*/"url",
    /*default_value=*/
    "https://onetimetoken.pa.googleapis.com/v1/onetimetokens:fetchEmail"};

}  // namespace one_time_tokens::features
