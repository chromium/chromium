// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INFERENCE_FEATURES_H_
#define COMPONENTS_PRIVATE_INFERENCE_FEATURES_H_

#include "base/features.h"
#include "base/metrics/field_trial_params.h"

namespace private_inference {

// The feature for private inference.
BASE_DECLARE_FEATURE(kPrivateInference);

// The API key for private inference.
extern const base::FeatureParam<std::string> kPrivateInferenceApiKey;

// Endpoint for private inference
extern const base::FeatureParam<std::string> kPrivateInferenceUrl;

}  // namespace private_inference

#endif  // COMPONENTS_PRIVATE_INFERENCE_FEATURES_H_
