// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_QUICK_DELETE_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_QUICK_DELETE_PROMO_H_

#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// 'QuickDeletePromo' is a class that represents an ephemeral home module,
// specifically designed for the quick delete promo card found in the
// educational tip section on chrome android. It is responsible for determining
// whether the module should be shown to the user based on the enable signal and
// the user's interaction history.
class QuickDeletePromo : public CardSelectionInfo {
 public:
  explicit QuickDeletePromo(PrefService* profile_prefs);
  ~QuickDeletePromo() override = default;

  static bool IsEnabled(bool is_in_enabled_cards_set, int impression_count);

  // CardSelectionInfo
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;

 private:
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_QUICK_DELETE_PROMO_H_
