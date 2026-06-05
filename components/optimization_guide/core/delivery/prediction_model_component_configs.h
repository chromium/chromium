// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MODEL_COMPONENT_CONFIGS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MODEL_COMPONENT_CONFIGS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Feature flag to control model delivery via Component Updater.
BASE_DECLARE_FEATURE(kPredictionModelComponentDelivery);

class PredictionModelComponentConfig {
 public:
  PredictionModelComponentConfig(std::string name,
                                 std::vector<uint8_t> public_key_sha256);
  ~PredictionModelComponentConfig();

  PredictionModelComponentConfig(const PredictionModelComponentConfig&);
  PredictionModelComponentConfig(PredictionModelComponentConfig&&);
  PredictionModelComponentConfig& operator=(
      const PredictionModelComponentConfig&);
  PredictionModelComponentConfig& operator=(PredictionModelComponentConfig&&);

  const std::string& component_name() const { return component_name_; }
  const std::vector<uint8_t>& public_key_sha256() const {
    return public_key_sha256_;
  }
  const std::string& component_id() const { return component_id_; }

 private:
  std::string component_name_;
  std::vector<uint8_t> public_key_sha256_;
  std::string component_id_;
};

// Returns the component configuration for the given prediction model target
// if it is supported for Component Updater delivery, or std::nullopt otherwise.
std::optional<PredictionModelComponentConfig> GetPredictionModelComponentConfig(
    proto::OptimizationTarget target);

// Returns all optimization targets that are configured for Component Updater
// delivery.
base::span<const proto::OptimizationTarget> GetPredictionModelTargets();

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MODEL_COMPONENT_CONFIGS_H_
