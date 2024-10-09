// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_EPHEMERAL_MODULE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_EPHEMERAL_MODULE_H_

#import <map>
#import <string>
#import <string_view>
#import <vector>

#import "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"

namespace segmentation_platform::home_modules {

class TipsEphemeralModule : public CardSelectionInfo {
 public:
  explicit TipsEphemeralModule() : CardSelectionInfo(kTipsEphemeralModule) {}
  ~TipsEphemeralModule() override = default;

  // Returns `true` if the given label corresponds to a `TipsEphemeralModule`
  // variation.
  static bool IsModuleLabel(std::string_view label);

  // `CardSelectionInfo` overrides.
  std::vector<std::string> OutputLabels() override;
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_EPHEMERAL_MODULE_H_
