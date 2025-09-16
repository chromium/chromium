// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_inference/features.h"

namespace private_inference {

BASE_FEATURE(kPrivateInference, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPrivateInferenceApiKey{
    &kPrivateInference, "api-key", ""};

const base::FeatureParam<std::string> kPrivateInferenceUrl{&kPrivateInference,
                                                           "url", ""};

}  // namespace private_inference
