// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CHROME_USER_ENGAGEMENT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CHROME_USER_ENGAGEMENT_H_

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Feature flag for enabling ChromeUserEngagement segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformChromeUserEngagement);

// Model to predict whether the user belongs to ChromeUserEngagement segment.
class ChromeUserEngagement : public DefaultModelProvider {
 public:
  static constexpr char kChromeUserEngagementKey[] = "chrome_user_engagement";
  static constexpr char kChromeUserEngagementUmaName[] = "ChromeUserEngagement";

  ChromeUserEngagement();
  ~ChromeUserEngagement() override = default;

  ChromeUserEngagement(const ChromeUserEngagement&) = delete;
  ChromeUserEngagement& operator=(const ChromeUserEngagement&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CHROME_USER_ENGAGEMENT_H_
