// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_COMMON_ONE_TIME_TOKEN_FEATURES_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_COMMON_ONE_TIME_TOKEN_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace one_time_tokens::features {

// All features in alphabetical order.

COMPONENT_EXPORT(ONE_TIME_TOKENS)
BASE_DECLARE_FEATURE(kGmailOtpRetrievalService);

}  // namespace one_time_tokens::features

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_COMMON_ONE_TIME_TOKEN_FEATURES_H_
