// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_H_

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// 'DefaultBrowserPromo' is a class that represents an ephemeral home module,
// specifically designed for the default browser promo card found in the
// educational tip section on chrome android. It is responsible for determining
// whether the module should be shown to the user based on the enable signal and
// the user's interaction history.
class DefaultBrowserPromo : public CardSelectionInfo {
 public:
  DefaultBrowserPromo();
  ~DefaultBrowserPromo() override = default;

  static bool IsEnabled(int impression_count);

  // CardSelectionInfo
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_H_
