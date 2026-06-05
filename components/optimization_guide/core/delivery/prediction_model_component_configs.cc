// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prediction_model_component_configs.h"

#include <array>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/crx_file/id_util.h"

namespace optimization_guide {

BASE_FEATURE(kPredictionModelComponentDelivery,
             base::FEATURE_DISABLED_BY_DEFAULT);

PredictionModelComponentConfig::PredictionModelComponentConfig(
    std::string name,
    std::vector<uint8_t> public_key_sha256)
    : component_name_(std::move(name)),
      public_key_sha256_(std::move(public_key_sha256)),
      component_id_(crx_file::id_util::GenerateIdFromHash(public_key_sha256_)) {
  CHECK_GE(public_key_sha256_.size(), crx_file::id_util::kIdSize);
}

PredictionModelComponentConfig::~PredictionModelComponentConfig() = default;

PredictionModelComponentConfig::PredictionModelComponentConfig(
    const PredictionModelComponentConfig&) = default;

PredictionModelComponentConfig::PredictionModelComponentConfig(
    PredictionModelComponentConfig&&) = default;

PredictionModelComponentConfig& PredictionModelComponentConfig::operator=(
    const PredictionModelComponentConfig&) = default;

PredictionModelComponentConfig& PredictionModelComponentConfig::operator=(
    PredictionModelComponentConfig&&) = default;

namespace {

// TODO: crbug.com/515762868 - Switch to a real key hash.
constexpr std::array<uint8_t, 32> kModelValidationPublicKeySHA256 = {
    0x25, 0xe5, 0x03, 0x34, 0x54, 0x52, 0x11, 0xb6, 0x26, 0xd8, 0x1d,
    0xed, 0x6b, 0x22, 0x15, 0x90, 0x9a, 0x44, 0xf0, 0x88, 0xdc, 0x19,
    0xfa, 0x5d, 0xd4, 0x55, 0xf7, 0x95, 0x88, 0xff, 0xfd, 0x8a};

struct ComponentConfigEntry {
  proto::OptimizationTarget target;
  const char* component_name;
  std::array<uint8_t, 32> public_key_sha256;
};

constexpr std::array kConfigs = {
    ComponentConfigEntry{proto::OPTIMIZATION_TARGET_MODEL_VALIDATION,
                         "Optimization Guide Model Validation",
                         kModelValidationPublicKeySHA256},
};

const ComponentConfigEntry* FindConfigEntry(proto::OptimizationTarget target) {
  for (const auto& entry : kConfigs) {
    if (entry.target == target) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace

std::optional<PredictionModelComponentConfig> GetPredictionModelComponentConfig(
    proto::OptimizationTarget target) {
  if (!base::FeatureList::IsEnabled(kPredictionModelComponentDelivery)) {
    return std::nullopt;
  }

  const ComponentConfigEntry* entry = FindConfigEntry(target);
  if (!entry) {
    return std::nullopt;
  }

  return PredictionModelComponentConfig(
      entry->component_name,
      std::vector<uint8_t>(entry->public_key_sha256.begin(),
                           entry->public_key_sha256.end()));
}

base::span<const proto::OptimizationTarget> GetPredictionModelTargets() {
  if (!base::FeatureList::IsEnabled(kPredictionModelComponentDelivery)) {
    return {};
  }
  static const base::NoDestructor<std::vector<proto::OptimizationTarget>>
      targets([] {
        std::vector<proto::OptimizationTarget> t;
        t.reserve(kConfigs.size());
        for (const auto& entry : kConfigs) {
          t.push_back(entry.target);
        }
        return t;
      }());
  return *targets;
}

}  // namespace optimization_guide
