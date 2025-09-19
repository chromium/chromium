// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TIPS_NOTIFICATIONS_RANKER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_TIPS_NOTIFICATIONS_RANKER_H_

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Input Features.

// Enum values for histograms.
inline constexpr std::array<int32_t, 1> kEnumValueForEnhancedSafeBrowsingUsage{
    /*EnhancedSafeBrowsing=*/1};

inline constexpr std::array<int32_t, 1>
    kEnumValueForQuickDeleteMagicStackImpression{/*QuickDelete=*/9};

// Set UMA metrics to use as input.
// TODO(crbug.com/445775311): IFTTT the metrics locations.
#define TIPS_NOTIFICATIONS_RANKER_FEATURES(F)                                  \
  F(kEnhancedSafeBrowsingUseCountIdx,                                          \
    SEGMENTATION_UMA_ENUM("SafeBrowsing.Settings.UserAction.Default", 28,      \
                          kEnumValueForEnhancedSafeBrowsingUsage.data(),       \
                          kEnumValueForEnhancedSafeBrowsingUsage.size()))      \
  F(kQuickDeleteMagicStackShownCountIdx,                                       \
    SEGMENTATION_UMA_ENUM(                                                     \
        "MagicStack.Clank.NewTabPage.Module.TopImpressionV2", 28,              \
        kEnumValueForQuickDeleteMagicStackImpression.data(),                   \
        kEnumValueForQuickDeleteMagicStackImpression.size()))                  \
  F(kGoogleLensNewTabPageUseCountIdx,                                          \
    SEGMENTATION_USER_ACTION("NewTabPage.SearchBox.Lens", 28))                 \
  F(kGoogleLensMobileOmniboxUseCountIdx,                                       \
    SEGMENTATION_USER_ACTION("MobileOmniboxLens", 28))                         \
  F(kGoogleLensTasksSurfaceUseCountIdx,                                        \
    SEGMENTATION_USER_ACTION("TasksSurface.FakeBox.Lens", 28))                 \
  F(kEnhancedSafeBrowsingIsEnabledIdx,                                         \
    SEGMENTATION_INPUT_CONTEXT(kEnhancedSafeBrowsingStatus))                   \
  F(kQuickDeleteWasEverUsedIdx, SEGMENTATION_INPUT_CONTEXT(kQuickDeleteUsage)) \
  F(kBottomOmniboxIsEnabledIdx,                                                \
    SEGMENTATION_INPUT_CONTEXT(kBottomOmniboxStatus))                          \
  F(kBottomOmniboxWasEverUsedIdx,                                              \
    SEGMENTATION_INPUT_CONTEXT(kBottomOmniboxUsage))

SEGMENTATION_DEFINE_FEATURES(kTipsNotificationsRankerFeatures,
                             TIPS_NOTIFICATIONS_RANKER_FEATURES);

// Model to predict whether the user belongs to TipsNotificationsRanker segment.
class TipsNotificationsRanker : public DefaultModelProvider {
 public:
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
