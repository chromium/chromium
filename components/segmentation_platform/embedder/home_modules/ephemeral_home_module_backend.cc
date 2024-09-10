// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::home_modules {

namespace {
using proto::SegmentId;

// Default parameters for EphemeralHomeModuleBackend model.
constexpr SegmentId kSegmentId = SegmentId::EPHEMERAL_HOME_MODULE_BACKEND;
// Default parameters for TestEphemeralHomeModuleBackend model.
constexpr SegmentId kTestSegmentId =
    SegmentId::EPHEMERAL_HOME_MODULE_BACKEND_TEST;
constexpr int64_t kModelVersion = 1;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 7 days of data.
constexpr int64_t kMinSignalCollectionLength = 0;
// Refresh the result every 7 days.
constexpr int64_t kResultTTLDays = 7;
// Number of labels to return to the caller.
constexpr size_t kMaxOutputLabelsToRank = 1;

constexpr char kEphemeralHomeModuleBackendUmaName[] =
    "EphemeralHomeModuleBackend";

}  // namespace

// static
std::unique_ptr<Config> EphemeralHomeModuleBackend::GetConfig(
    HomeModulesCardRegistry* home_modules_card_registry) {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformEphemeralCardRanker)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kEphemeralHomeModuleBackendKey;
  config->segmentation_uma_name = kEphemeralHomeModuleBackendUmaName;
  config->AddSegmentId(kSegmentId,
                       std::make_unique<EphemeralHomeModuleBackend>(
                           home_modules_card_registry->GetWeakPtr()));
  config->auto_execute_and_cache = false;
  return config;
}

EphemeralHomeModuleBackend::EphemeralHomeModuleBackend(
    base::WeakPtr<HomeModulesCardRegistry> home_modules_card_registry)
    : DefaultModelProvider(kSegmentId),
      home_modules_card_registry_(home_modules_card_registry) {}
EphemeralHomeModuleBackend::~EphemeralHomeModuleBackend() = default;

std::unique_ptr<DefaultModelProvider::ModelConfig>
EphemeralHomeModuleBackend::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  if (!home_modules_card_registry_) {
    return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
  }
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  // Set output config.
  const std::vector<std::string>& output_labels =
      home_modules_card_registry_->all_output_labels();
  // Use a threshold slightly greater than the kNotShown value since the
  // threshold checks for strictly less than value.
  float dont_show_threshold =
      EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kNotShown) +
      0.001f;
  writer.AddOutputConfigForMultiClassClassifier(
      output_labels, kMaxOutputLabelsToRank, dont_show_threshold);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLDays, proto::TimeUnit::DAY);

  // Set features.
  const auto& all_cards =
      home_modules_card_registry_->get_all_cards_by_priority();
  for (const auto& card : all_cards) {
    const auto& inputs = card->GetInputs();
    for (const auto& key_and_input : inputs) {
      const FeatureQuery& input = key_and_input.second;
      if (input.uma_feature) {
        writer.AddUmaFeatures(&input.uma_feature.value(), 1);
      } else if (input.sql_feature) {
        writer.AddSqlFeature(*input.sql_feature);
      } else if (input.custom_input) {
        writer.AddCustomInput(*input.custom_input);
      }
    }
  }

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void EphemeralHomeModuleBackend::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  if (!home_modules_card_registry_ ||
      inputs.size() != home_modules_card_registry_->all_cards_input_size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  const AllCardSignals all_signals(
      home_modules_card_registry_->get_card_signal_map(), std::move(inputs));

  const auto& all_cards =
      home_modules_card_registry_->get_all_cards_by_priority();
  const float dont_show_result =
      EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kNotShown);
  ModelProvider::Request result(
      home_modules_card_registry_->all_output_labels().size(),
      dont_show_result);

  for (const auto& card : all_cards) {
    CardSelectionSignals card_signals(&all_signals, card->card_name());
    CardSelectionInfo::ShowResult card_result =
        card->ComputeCardResult(card_signals);
    if (card_result.position == EphemeralHomeModuleRank::kNotShown) {
      continue;
    }
    std::string label = card->card_name();
    if (card_result.result_label) {
      label = *card_result.result_label;
    }
    result[home_modules_card_registry_->get_label_index(
        *card_result.result_label)] =
        EphemeralHomeModuleRankToScore(card_result.position);
    // This assumes only one card is shown, if more than one card need to be
    // shown, update the priority of cards using EphemeralHomeModuleRank and
    // wait for 2 cards here.
    break;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

// static
std::unique_ptr<Config> TestEphemeralHomeModuleBackend::GetConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kEphemeralHomeModuleBackendKey;
  config->segmentation_uma_name = kEphemeralHomeModuleBackendUmaName;
  config->AddSegmentId(kTestSegmentId,
                       std::make_unique<TestEphemeralHomeModuleBackend>());
  config->auto_execute_and_cache = false;
  return config;
}

TestEphemeralHomeModuleBackend::TestEphemeralHomeModuleBackend()
    : DefaultModelProvider(kTestSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
TestEphemeralHomeModuleBackend::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  writer.AddOutputConfigForMultiClassClassifier(
      {kPriceTrackingNotificationPromo}, kMaxOutputLabelsToRank,
      EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kNotShown));
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLDays, proto::TimeUnit::DAY);

  writer.AddFromInputContext("is_new_user", segmentation_platform::kIsNewUser);
  writer.AddFromInputContext("is_synced", segmentation_platform::kIsSynced);

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void TestEphemeralHomeModuleBackend::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  const float dont_show_result =
      EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kNotShown);
  ModelProvider::Request response(1, dont_show_result);
  std::string card_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kEphemeralModuleBackendRankerTestOverride);
  if (card_type == "price_tracking_notification_promo") {
    response[0] = 1;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

}  // namespace segmentation_platform::home_modules
