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

constexpr std::array<uint8_t, 32>
    kGeolocationPermissionPredictionsPublicKeySHA256 = {
        0xaf, 0x54, 0x45, 0x0b, 0x34, 0x12, 0x2a, 0xfa, 0x04, 0x4b, 0xf8,
        0x42, 0x19, 0x5f, 0x1c, 0xb4, 0xcd, 0xac, 0x78, 0x1e, 0xb9, 0xdf,
        0xf4, 0x51, 0x83, 0x56, 0x3f, 0x5f, 0x9a, 0x25, 0x91, 0x55};

constexpr std::array<uint8_t, 32>
    kNotificationPermissionPredictionsPublicKeySHA256 = {
        0x5c, 0xa4, 0x63, 0x5b, 0x85, 0x0a, 0xce, 0xb5, 0x2c, 0xf1, 0xe7,
        0x97, 0x6f, 0x82, 0x51, 0xeb, 0xe1, 0x95, 0x60, 0xc6, 0xd7, 0x91,
        0xe6, 0x9f, 0xdc, 0x40, 0xce, 0x38, 0x30, 0xd2, 0x1a, 0x30};

constexpr std::array<uint8_t, 32>
    kClientSidePhishingImageEmbedderPublicKeySHA256 = {
        0x27, 0x95, 0x4b, 0x35, 0x18, 0x93, 0x13, 0x4e, 0x99, 0x7e, 0x3d,
        0x81, 0x9d, 0x0e, 0x7f, 0x37, 0x4f, 0x32, 0xc2, 0x3d, 0x1c, 0x4d,
        0x4b, 0x2f, 0x9e, 0xb8, 0x1d, 0xff, 0xf7, 0xa1, 0xf2, 0x50};

struct ComponentConfigEntry {
  proto::OptimizationTarget target;
  const char* component_name;
  std::array<uint8_t, 32> public_key_sha256;
};

constexpr std::array kConfigs = {
    ComponentConfigEntry{
        proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS,
        "Optimization Guide Geolocation Permission Predictions",
        kGeolocationPermissionPredictionsPublicKeySHA256},
    ComponentConfigEntry{
        proto::OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS,
        "Optimization Guide Notification Permission Predictions",
        kNotificationPermissionPredictionsPublicKeySHA256},
    ComponentConfigEntry{
        proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER,
        "Optimization Guide Client Side Phishing Image Embedder",
        kClientSidePhishingImageEmbedderPublicKeySHA256},
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
