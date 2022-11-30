// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DELEGATED_INK_PREDICTION_CONFIGURATION_H_
#define COMPONENTS_VIZ_COMMON_DELEGATED_INK_PREDICTION_CONFIGURATION_H_

namespace viz {

struct PredictionConfiguration {
  // The number of points to predict into the future when prediction is
  // available.
  const int points_to_predict;
  // The time that each predicted point should be ahead of the previous point,
  // in milliseconds.
  const int milliseconds_into_future_per_point;
};

// The prediction configurations that are being tested.
constexpr PredictionConfiguration kPredictionConfigs[] = {{1, 12},
                                                          {2, 6},
                                                          {1, 6},
                                                          {2, 3}};

// Current number of different prediction configurations that are being tested.
constexpr int kNumberOfPredictionConfigs = std::size(kPredictionConfigs);

// Indicates which element of |kPredictionConfigs| is being selected in
// tests and viz/common/features.cc
enum PredictionConfig {
  k1Point12Ms = 0,
  k2Points6Ms = 1,
  k1Point6Ms = 2,
  k2Points3Ms = 3
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DELEGATED_INK_PREDICTION_CONFIGURATION_H_
