// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_ENHANCED_SAFE_BROWSING_EPHEMERAL_MODULE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_ENHANCED_SAFE_BROWSING_EPHEMERAL_MODULE_H_

#include <map>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"

namespace segmentation_platform::home_modules {

// `EnhancedSafeBrowsingEphemeralModule` is a class that represents an
// ephemeral Magic Stack module for the Enhanced Safe Browsing tip. It is
// responsible for determining whether the module should be shown to the user
// based on various signals and the user's interaction history.
class EnhancedSafeBrowsingEphemeralModule : public CardSelectionInfo {
 public:
  explicit EnhancedSafeBrowsingEphemeralModule(PrefService* profile_prefs)
      : CardSelectionInfo(kEnhancedSafeBrowsingEphemeralModule),
        profile_prefs_(profile_prefs) {}
  ~EnhancedSafeBrowsingEphemeralModule() override = default;

  // Returns `true` if the given label corresponds to an
  // `EnhancedSafeBrowsingEphemeralModule` variation.
  static bool IsModuleLabel(std::string_view label);

  // Returns `true` if the `EnhancedSafeBrowsingEphemeralModule` should be
  // enabled, considering the given impression count.
  static bool IsEnabled(int impression_count);

  // `CardSelectionInfo` overrides.
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;

 private:
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_ENHANCED_SAFE_BROWSING_EPHEMERAL_MODULE_H_
