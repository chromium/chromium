// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_TEST_SUPPORT_MACHINE_LEARNING_TEST_UTILS_H_
#define CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_TEST_SUPPORT_MACHINE_LEARNING_TEST_UTILS_H_

#include <memory>

#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"

namespace optimization_guide {
namespace proto {
class PredictionModel;
}  // namespace proto
}  // namespace optimization_guide

namespace machine_learning {
namespace testing {

const double kModelThreshold = 5.0;
const double kModelWeight = 3.0;
const double kModelValueDiff = 2.0;

// Returns a single-leaf decision tree model proto which will yield the given
// |decision| when evaluated.
std::unique_ptr<optimization_guide::proto::PredictionModel>
GetModelProtoForPredictionResult(mojom::DecisionTreePredictionResult decision);

}  // namespace testing
}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_TEST_SUPPORT_MACHINE_LEARNING_TEST_UTILS_H_
