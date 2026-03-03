// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_PRICE_TRACKING_NOTIFICATION_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_PRICE_TRACKING_NOTIFICATION_PROMO_H_

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

class PrefRegistrySimple;
class PrefService;

namespace segmentation_platform::home_modules {

// Signal Keys for this card.
extern const char kHasSubscriptionSignalKey[];
extern const char kIsNewUserSignalKey[];
extern const char kIsSyncedSignalKey[];

class PriceTrackingNotificationPromo : public CardSelectionInfo {
 public:
  PriceTrackingNotificationPromo();
  ~PriceTrackingNotificationPromo() override = default;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static bool IsEnabled(PrefService* profile_prefs);

  // `CardSelectionInfo` overrides.
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
  void OnShow(PrefService* profile_prefs, PrefService* local_state) override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_PRICE_TRACKING_NOTIFICATION_PROMO_H_
