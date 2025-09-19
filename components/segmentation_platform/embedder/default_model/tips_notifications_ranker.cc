// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/tips_notifications_ranker.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for TipsNotificationsRanker model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_TIPS_NOTIFICATIONS_RANKER;
constexpr int64_t kModelVersion = 2;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 7 days of data.
constexpr int64_t kMinSignalCollectionLength = 7;

// Labels for the classification output.
#define TIPS_NOTIFICATIONS_LABELS(F)                    \
  F(kEnhancedSafeBrowsingTipIdx, kEnhancedSafeBrowsing) \
  F(kQuickDeleteTipIdx, kQuickDelete)                   \
  F(kGoogleLensTipIdx, kGoogleLens)                     \
  F(kBottomOmniboxTipIdx, kBottomOmnibox)

SEGMENTATION_DEFINE_LABELS(kTipsNotificationsLabels, TIPS_NOTIFICATIONS_LABELS);

std::vector<int> GetTipsPriorityRankingList() {
  std::vector<int> tips_list;
  // Define the priority ranking based on the feature param.
  // First in the list represents highest priority and last is lowest.
  if (features::kTrustAndSafety.Get()) {
    tips_list.emplace_back(kEnhancedSafeBrowsingTipIdx);
    tips_list.emplace_back(kQuickDeleteTipIdx);
    tips_list.emplace_back(kGoogleLensTipIdx);
    tips_list.emplace_back(kBottomOmniboxTipIdx);
  } else if (features::kEssential.Get()) {
    tips_list.emplace_back(kQuickDeleteTipIdx);
    tips_list.emplace_back(kBottomOmniboxTipIdx);
    tips_list.emplace_back(kEnhancedSafeBrowsingTipIdx);
    tips_list.emplace_back(kGoogleLensTipIdx);
  } else if (features::kNewFeatures.Get()) {
    tips_list.emplace_back(kGoogleLensTipIdx);
    tips_list.emplace_back(kBottomOmniboxTipIdx);
    tips_list.emplace_back(kQuickDeleteTipIdx);
    tips_list.emplace_back(kEnhancedSafeBrowsingTipIdx);
  }
  return tips_list;
}

bool IsEnhancedSafeBrowsingTipEligible(float is_enabled, float use_count) {
  return is_enabled == 0 && use_count == 0;
}

bool IsQuickDeleteTipEligible(float was_ever_used,
                              float magic_stack_shown_count) {
  return was_ever_used == 0 && magic_stack_shown_count == 0;
}

bool IsGoogleLensTipEligible(float ntp_use_count,
                             float omnibox_use_count,
                             float tasks_surface_use_count) {
  return ntp_use_count == 0 && omnibox_use_count == 0 &&
         tasks_surface_use_count == 0;
}

bool IsBottomOmniboxTipEligible(float is_enabled, float was_ever_used) {
  return is_enabled == 0 && was_ever_used == 0;
}

}  // namespace

// static
std::unique_ptr<Config> TipsNotificationsRanker::GetConfig() {
  if (!base::FeatureList::IsEnabled(features::kAndroidTipsNotifications)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kTipsNotificationsRankerKey;
  config->segmentation_uma_name = kTipsNotificationsRankerUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<TipsNotificationsRanker>());
  config->auto_execute_and_cache = false;
  return config;
}

TipsNotificationsRanker::TipsNotificationsRanker()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
TipsNotificationsRanker::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);
  metadata.set_upload_tensors(false);

  // Set output config.
  writer.AddOutputConfigForMultiClassClassifier(kTipsNotificationsLabels,
                                                kLabelsCount,
                                                /*threshold=*/0.5);

  // Set UMA features and custom inputs.
  writer.AddFeatures(base::span(kTipsNotificationsRankerFeatures));

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void TipsNotificationsRanker::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  ModelProvider::Response response(kLabelsCount, 0);
  // TODO(crbug.com/444281425): Include logic for trying to schedule once a week
  // and to cycle the tips via histogram on notif showing for L28 or max 1 time.
  // Counts refer to the L28 days and bools are represented through 0 or 1.
  float esb_is_enabled = inputs[kEnhancedSafeBrowsingIsEnabledIdx];
  float esb_use_count = inputs[kEnhancedSafeBrowsingUseCountIdx];
  float qd_ever_used = inputs[kQuickDeleteWasEverUsedIdx];
  float qd_magic_stack_shown_count =
      inputs[kQuickDeleteMagicStackShownCountIdx];
  float lens_ntp_use_count = inputs[kGoogleLensNewTabPageUseCountIdx];
  float lens_omnibox_use_count = inputs[kGoogleLensMobileOmniboxUseCountIdx];
  float lens_tasks_surface_use_count =
      inputs[kGoogleLensTasksSurfaceUseCountIdx];
  float bottom_omnibox_is_enabled = inputs[kBottomOmniboxIsEnabledIdx];
  float bottom_omnibox_was_ever_used = inputs[kBottomOmniboxWasEverUsedIdx];

  // Cycle through the priority list and mark the highest ranked eligible tip to
  // show if it exists and then early exit.
  std::vector<int> tips_priority_list = GetTipsPriorityRankingList();
  if (!tips_priority_list.empty()) {
    bool hasEligibleTip = false;
    for (auto tip_idx : tips_priority_list) {
      switch (tip_idx) {
        case kEnhancedSafeBrowsingTipIdx:
          if (IsEnhancedSafeBrowsingTipEligible(esb_is_enabled,
                                                esb_use_count)) {
            response[kEnhancedSafeBrowsingTipIdx] = 1;
            hasEligibleTip = true;
          }
          break;
        case kQuickDeleteTipIdx:
          if (IsQuickDeleteTipEligible(qd_ever_used,
                                       qd_magic_stack_shown_count)) {
            response[kQuickDeleteTipIdx] = 1;
            hasEligibleTip = true;
          }
          break;
        case kGoogleLensTipIdx:
          if (IsGoogleLensTipEligible(lens_ntp_use_count,
                                      lens_omnibox_use_count,
                                      lens_tasks_surface_use_count)) {
            response[kGoogleLensTipIdx] = 1;
            hasEligibleTip = true;
          }
          break;
        case kBottomOmniboxTipIdx:
          if (IsBottomOmniboxTipEligible(bottom_omnibox_is_enabled,
                                         bottom_omnibox_was_ever_used)) {
            response[kBottomOmniboxTipIdx] = 1;
            hasEligibleTip = true;
          }
          break;
        default:
          NOTREACHED();
      }

      // Early exit if an eligible tip has been found.
      if (hasEligibleTip) {
        break;
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}
}  // namespace segmentation_platform
