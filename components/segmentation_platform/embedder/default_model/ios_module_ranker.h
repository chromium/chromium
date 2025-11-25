// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_IOS_MODULE_RANKER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_IOS_MODULE_RANKER_H_

#include <memory>

#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Model to predict whether the user belongs to IosModuleRanker segment.
class IosModuleRanker : public DefaultModelProvider {
 public:
  enum Label {
    kLabelMostVisitedTiles,
    kLabelShortcuts,
    kLabelSafetyCheck,
    kLabelTabResumption,
    kLabelParcelTracking,
    kLabelShopCard,
    kLabelCount
  };
  enum Feature {
    // UMA features
    kFeatureMVTClick7Days,
    kFeatureMVTImpression7Days,
    kFeatureShortcutsClick7Days,
    kFeatureShortcutsImpression7Days,
    kFeatureSafetyCheckClick7Days,
    kFeatureSafetyCheckImpression7Days,
    kFeatureMVTClick28Days,
    kFeatureMVTImpression28Days,
    kFeatureShortcutsClick28Days,
    kFeatureShortcutsImpression28Days,
    kFeatureSafetyCheckClick28Days,
    kFeatureSafetyCheckImpression28Days,
    kFeatureOpenMVT7Days,
    kFeatureOpenMVT28Days,
    kFeatureBookmarkManager7Days,
    kFeatureBookmarkManager28Days,
    kFeatureReadingList7Days,
    kFeatureReadingList28Days,
    kFeatureMobileReadingListOpen7Days,
    kFeatureMobileReadingListOpen28Days,
    kFeatureMobileReadingListAdd7Days,
    kFeatureMobileReadingListAdd28Days,
    kFeatureTabResumptionClick7Days,
    kFeatureTabResumptionImpression7Days,
    kFeatureTabResumptionClick28Days,
    kFeatureTabResumptionImpression28Days,
    kFeatureParcelTrackingClick7Days,
    kFeatureParcelTrackingImpression7Days,
    kFeatureParcelTrackingClick28Days,
    kFeatureParcelTrackingImpression28Days,
    kFeatureShopCardClick7Days,
    kFeatureShopCardImpression7Days,
    kFeatureShopCardClick28Days,
    kFeatureShopCardImpression28Days,

    // Custom inputs
    kFeatureMostVisitedTilesFreshness,
    kFeatureShortcutsFreshness,
    kFeatureSafetyCheckFreshness,
    kFeatureTabResumptionFreshness,
    kFeatureParcelTrackingFreshness,
    kFeatureShopCardFreshness,
    kFeatureCount
  };

  IosModuleRanker();
  ~IosModuleRanker() override = default;

  IosModuleRanker(const IosModuleRanker&) = delete;
  IosModuleRanker& operator=(const IosModuleRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

// Test model for IosModuleRanker that uses same config, but just gives the card
// passed through the '--test-ios-module-ranker' commandline arg a top score.
class TestIosModuleRanker : public DefaultModelProvider {
 public:
  TestIosModuleRanker();
  ~TestIosModuleRanker() override = default;

  TestIosModuleRanker(const TestIosModuleRanker&) = delete;
  TestIosModuleRanker& operator=(const TestIosModuleRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_IOS_MODULE_RANKER_H_
