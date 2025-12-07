// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_APP_BUNDLE_PROMO_EPHEMERAL_MODULE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_APP_BUNDLE_PROMO_EPHEMERAL_MODULE_H_

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// Signal Keys for this card.
extern const char kAppBundleAppsInstalledCountSignalKey[];

// `AppBundlePromoEphemeralModule` is a class that represents an ephemeral
// Magic Stack module for the App Bundle promo. It is responsible for
// determining whether the module should be shown to the user based on various
// signals and card impression count, and the user's interaction history.
class AppBundlePromoEphemeralModule : public CardSelectionInfo {
 public:
  explicit AppBundlePromoEphemeralModule();
  ~AppBundlePromoEphemeralModule() override = default;

  // Returns `true` if the given label corresponds to an
  // `AppBundlePromoEphemeralModule` variation.
  static bool IsModuleLabel(std::string_view label);

  // Returns `true` if the `AppBundlePromoEphemeralModule` should be enabled,
  // considering the given impression count.
  static bool IsEnabled(int impression_count);

  // CardSelectionInfo
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_APP_BUNDLE_PROMO_EPHEMERAL_MODULE_H_
