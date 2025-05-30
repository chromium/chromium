// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HISTORY_SYNC_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HISTORY_SYNC_PROMO_H_

#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// 'HistorySyncPromo' is a class that represents an ephemeral home module,
// specifically designed for the history sync promo card found in the
// educational tip section on Android. It is responsible for determining
// whether the module should be shown to the user based on the enable signal and
// the user's interaction history.
class HistorySyncPromo : public CardSelectionInfo {
 public:
  explicit HistorySyncPromo(PrefService* profile_prefs);
  ~HistorySyncPromo() override = default;

  static bool IsEnabled(bool is_in_enabled_cards_set, int impression_count);

  // CardSelectionInfo
  std::map<SignalKey, FeatureQuery> GetInputs() override;

  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;

 private:
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HISTORY_SYNC_PROMO_H_
