// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_RANK_FETCHER_HELPER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_RANK_FETCHER_HELPER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform::home_modules {

// Helper class to fetch rank from stable and ephemeral modules and merge them.
class RankFetcherHelper {
 public:
  RankFetcherHelper();
  ~RankFetcherHelper();

  RankFetcherHelper(const RankFetcherHelper&) = delete;
  RankFetcherHelper& operator=(const RankFetcherHelper&) = delete;

  // Get final list of modules to show on NTP.
  void GetHomeModulesRank(SegmentationPlatformService* segmentation_service,
                          const PredictionOptions& modules_prediction_options,
                          scoped_refptr<InputContext> input_context,
                          ClassificationResultCallback callback);

 private:
  void OnGotModuleInputKeys(SegmentationPlatformService* segmentation_service,
                            const PredictionOptions& module_prediction_options,
                            scoped_refptr<InputContext> input_context,
                            ClassificationResultCallback callback,
                            std::set<std::string> module_input_keys);

  void OnGotEphemeralInputKeys(
      SegmentationPlatformService* segmentation_service,
      const PredictionOptions& module_prediction_options,
      scoped_refptr<InputContext> input_context,
      ClassificationResultCallback callback,
      std::set<std::string> module_input_keys,
      std::set<std::string> ephemeral_input_keys);

  void OnGetModulesRank(SegmentationPlatformService* segmentation_service,
                        scoped_refptr<InputContext> input_context,
                        ClassificationResultCallback callback,
                        const ClassificationResult& result);

  void OnGetEphemeralRank(SegmentationPlatformService* segmentation_service,
                          scoped_refptr<InputContext> input_context,
                          ClassificationResultCallback callback,
                          const ClassificationResult& modules_rank);

  void MergeResultsAndRunCallback(const ClassificationResult& modules_rank,
                                  ClassificationResultCallback callback,
                                  const AnnotatedNumericResult& ephemeral_rank);

  base::WeakPtrFactory<RankFetcherHelper> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_RANK_FETCHER_HELPER_H_
