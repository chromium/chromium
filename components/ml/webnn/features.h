// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ML_WEBNN_FEATURES_H_
#define COMPONENTS_ML_WEBNN_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace webnn::features {

// Enables the Web Machine Learning Neural Network Service to access hardware
// acceleration out of renderer process. Explainer:
// https://github.com/webmachinelearning/webnn/blob/main/explainer.md
COMPONENT_EXPORT(WEBNN_FEATURES)
BASE_DECLARE_FEATURE(kEnableMachineLearningNeuralNetworkService);

}  // namespace webnn::features

#endif  // COMPONENTS_ML_WEBNN_FEATURES_H_
