// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TIPS_NOTIFICATIONS_RANKER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TIPS_NOTIFICATIONS_RANKER_H_

#include <memory>

#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Model to predict whether the user belongs to TipsNotificationsRanker segment.
class TipsNotificationsRanker : public DefaultModelProvider {
 public:
  enum Label {
    kEnhancedSafeBrowsingTipIdx,
    kQuickDeleteTipIdx,
    kGoogleLensTipIdx,
    kBottomOmniboxTipIdx,
    kLabelCount
  };

  enum Feature {
    kEnhancedSafeBrowsingUseCountIdx,
    kQuickDeleteMagicStackShownCountIdx,
    kGoogleLensNewTabPageUseCountIdx,
    kGoogleLensMobileOmniboxUseCountIdx,
    kGoogleLensTasksSurfaceUseCountIdx,
    kGoogleLensTipsNotificationsUseCountIdx,
    kEnhancedSafeBrowsingIsEnabledIdx,
    kQuickDeleteWasEverUsedIdx,
    kBottomOmniboxIsEnabledIdx,
    kBottomOmniboxWasEverUsedIdx,
    kAllFeatureTipsShownCountIdx,
    kEnhancedSafeBrowsingTipShownIdx,
    kQuickDeleteTipShownIdx,
    kGoogleLensTipShownIdx,
    kBottomOmniboxTipShownIdx,
    kFeatureCount
  };

  TipsNotificationsRanker();
  ~TipsNotificationsRanker() override = default;

  TipsNotificationsRanker(const TipsNotificationsRanker&) = delete;
  TipsNotificationsRanker& operator=(const TipsNotificationsRanker&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TIPS_NOTIFICATIONS_RANKER_H_
