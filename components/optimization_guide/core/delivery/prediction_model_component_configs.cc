// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prediction_model_component_configs.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/crx_file/id_util.h"

namespace optimization_guide {

BASE_FEATURE(kPredictionModelComponentDelivery,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPredictionModelTargets{
    &kPredictionModelComponentDelivery, "targets", ""};

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

constexpr std::array<uint8_t, 32> kClientSidePhishingPublicKeySHA256 = {
    0xec, 0x22, 0xd3, 0x7b, 0xe4, 0xb8, 0xca, 0x40, 0x26, 0x95, 0x90,
    0x8f, 0x97, 0xdd, 0xe6, 0xc1, 0x01, 0x6e, 0x7a, 0xc3, 0x0c, 0x61,
    0xa6, 0xcd, 0xa6, 0xa6, 0x95, 0xa3, 0x5a, 0xc4, 0x14, 0xe3};

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
    ComponentConfigEntry{proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
                         "Optimization Guide Client Side Phishing",
                         kClientSidePhishingPublicKeySHA256},
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

std::optional<proto::OptimizationTarget> ParseOptimizationTarget(
    std::string_view target_str) {
  int target_num;
  if (!base::StringToInt(target_str, &target_num)) {
    return std::nullopt;
  }
  if (!proto::OptimizationTarget_IsValid(target_num)) {
    return std::nullopt;
  }
  return static_cast<proto::OptimizationTarget>(target_num);
}

bool IsTargetEnabledForComponentDelivery(proto::OptimizationTarget target) {
  return std::ranges::contains(GetPredictionModelTargets(), target);
}

}  // namespace

std::optional<PredictionModelComponentConfig> GetPredictionModelComponentConfig(
    proto::OptimizationTarget target) {
  if (!IsTargetEnabledForComponentDelivery(target)) {
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

std::vector<proto::OptimizationTarget> GetPredictionModelTargets() {
  std::vector<proto::OptimizationTarget> targets;
  if (!base::FeatureList::IsEnabled(kPredictionModelComponentDelivery)) {
    return targets;
  }

  const std::string targets_param = kPredictionModelTargets.Get();
  if (targets_param.empty()) {
    return targets;
  }

  std::vector<proto::OptimizationTarget> parsed_targets;
  for (std::string_view target_str :
       base::SplitStringPiece(targets_param, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (std::optional<proto::OptimizationTarget> parsed_target =
            ParseOptimizationTarget(target_str)) {
      parsed_targets.push_back(*parsed_target);
    }
  }

  for (const auto& entry : kConfigs) {
    if (std::ranges::contains(parsed_targets, entry.target)) {
      targets.push_back(entry.target);
    }
  }
  return targets;
}

}  // namespace optimization_guide
