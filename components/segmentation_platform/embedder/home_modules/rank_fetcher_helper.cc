// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/rank_fetcher_helper.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform::home_modules {
namespace {

std::vector<std::string> GetFixedModuleList() {
#if BUILDFLAG(IS_IOS)
  return {};
#elif BUILDFLAG(IS_ANDROID)
  return {kPriceChange, kSingleTab, kSafetyHub, kAuxiliarySearch};
#else
  return {
      kPriceChange, kSingleTab, kSafetyHub, kAuxiliarySearch, kTabResumption
  };
#endif
}

#if BUILDFLAG(IS_IOS)
constexpr auto* kModuleFeatureFlag =
    &features::kSegmentationPlatformIosModuleRanker;
const char* kModuleRankerKey = kIosModuleRankerKey;
#else
constexpr auto* kModuleFeatureFlag =
    &features::kSegmentationPlatformAndroidHomeModuleRanker;
const char* kModuleRankerKey = kAndroidHomeModuleRankerKey;
#endif

void RunFixedRankingResult(ClassificationResultCallback callback) {
  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels = GetFixedModuleList();
  std::move(callback).Run(ClassificationResult(result));
}

}  // namespace

RankFetcherHelper::RankFetcherHelper() = default;
RankFetcherHelper::~RankFetcherHelper() = default;

void RankFetcherHelper::GetHomeModulesRank(
    SegmentationPlatformService* segmentation_service,
    const PredictionOptions& module_prediction_options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback) {
  if (!base::FeatureList::IsEnabled(*kModuleFeatureFlag)) {
    RunFixedRankingResult(std::move(callback));
    return;
  }

  segmentation_service->GetInputKeysForModel(
      kModuleRankerKey,
      base::BindOnce(&RankFetcherHelper::OnGotModuleInputKeys,
                     weak_ptr_factory_.GetWeakPtr(),
                     // The callback is run by segmentation service, so this
                     // pointer will be alive.
                     base::Unretained(segmentation_service),
                     module_prediction_options, input_context,
                     std::move(callback)));
}

void RankFetcherHelper::OnGotModuleInputKeys(
    SegmentationPlatformService* segmentation_service,
    const PredictionOptions& module_prediction_options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback,
    std::set<std::string> module_input_keys) {
  auto fetch_rank = base::BindOnce(
      &RankFetcherHelper::OnGotEphemeralInputKeys,
      weak_ptr_factory_.GetWeakPtr(),
      // The callback is run by segmentation service, so this pointer will be
      // alive.
      base::Unretained(segmentation_service), module_prediction_options,
      input_context, std::move(callback), std::move(module_input_keys));
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformEphemeralCardRanker)) {
    std::move(fetch_rank).Run({});
    return;
  }

  segmentation_service->GetInputKeysForModel(kEphemeralHomeModuleBackendKey,
                                             std::move(fetch_rank));
}

void RankFetcherHelper::OnGotEphemeralInputKeys(
    SegmentationPlatformService* segmentation_service,
    const PredictionOptions& module_prediction_options,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback,
    std::set<std::string> module_input_keys,
    std::set<std::string> ephemeral_input_keys) {
  module_input_keys.merge(std::move(ephemeral_input_keys));
  for (const std::string& input : module_input_keys) {
    if (!input_context->metadata_args.contains(input)) {
#if BUILDFLAG(IS_ANDROID)
      // Set missing "freshness" inputs to -1, in case any module is disabled.
      // This would lead to errors if required inputs were not passed in. Client
      // code should make sure all inputs are set correctly.
      if (input.find("freshness") != std::string::npos) {
        DLOG(WARNING) << "home modules missing input " << input << " using -1.";
        input_context->metadata_args.emplace(
            input, processing::ProcessedValue::FromFloat(-1));
      }
#endif
    }
  }

  segmentation_service->GetClassificationResult(
      kModuleRankerKey, module_prediction_options, input_context,
      base::BindOnce(&RankFetcherHelper::OnGetModulesRank,
                     weak_ptr_factory_.GetWeakPtr(),
                     // The callback is run by segmentation service, so this
                     // pointer will be alive.
                     base::Unretained(segmentation_service), input_context,
                     std::move(callback)));
}

void RankFetcherHelper::OnGetModulesRank(
    SegmentationPlatformService* segmentation_service,
    scoped_refptr<InputContext> input_context,
    ClassificationResultCallback callback,
    const ClassificationResult& modules_rank) {
  if (modules_rank.status != PredictionStatus::kSucceeded) {
    RunFixedRankingResult(std::move(callback));
    return;
  }

  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformEphemeralCardRanker)) {
    std::move(callback).Run(modules_rank);
    return;
  }

  segmentation_service->GetAnnotatedNumericResult(
      kEphemeralHomeModuleBackendKey, PredictionOptions(true), input_context,
      base::BindOnce(&RankFetcherHelper::MergeResultsAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), modules_rank,
                     std::move(callback)));
}

void RankFetcherHelper::MergeResultsAndRunCallback(
    const ClassificationResult& modules_rank,
    ClassificationResultCallback callback,
    const AnnotatedNumericResult& ephemeral_rank) {
  if (ephemeral_rank.status != PredictionStatus::kSucceeded) {
    std::move(callback).Run(modules_rank);
    return;
  }
  std::vector<std::string> merged_labels = modules_rank.ordered_labels;
  const auto& ephmeral_rank_scores = ephemeral_rank.GetAllResults();
  // TODO(ssid): This should just merge the labels based on scores. Use raw
  // result for both queries.
  for (const auto& it : ephmeral_rank_scores) {
    if (it.second >=
        EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kTop)) {
      merged_labels.insert(merged_labels.begin(), it.first);
    } else if (it.second >=
               EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kLast)) {
      merged_labels.push_back(it.first);
    }
  }

  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels = std::move(merged_labels);
  VLOG(1) << "Home modules segmentation result " << result.ToDebugString();
  std::move(callback).Run(result);
}

}  // namespace segmentation_platform::home_modules
