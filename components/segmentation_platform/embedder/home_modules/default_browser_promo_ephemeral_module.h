// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_EPHEMERAL_MODULE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_EPHEMERAL_MODULE_H_

#include <string_view>

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// Signal key for this card.
extern const char kIsDefaultBrowserSignalKey[];

// `DefaultBrowserPromoEphemeralModule` is a class that represents an ephemeral
// Magic Stack module for the Default Browser promo. It is responsible for
// determining whether the module should be shown to the user based on if the
// user has Chrome as their default browser.
class DefaultBrowserPromoEphemeralModule : public CardSelectionInfo {
 public:
  explicit DefaultBrowserPromoEphemeralModule();
  ~DefaultBrowserPromoEphemeralModule() override = default;

  // Returns `true` if the given label corresponds to a
  // `DefaultBrowserPromoEphemeralModule` variation.
  static bool IsModuleLabel(std::string_view label);

  // Returns `true` if the `DefaultBrowserPromoEphemeralModule` should be
  // enabled.
  static bool IsEnabled(int impression_count);

  // CardSelectionInfo
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_EPHEMERAL_MODULE_H_
