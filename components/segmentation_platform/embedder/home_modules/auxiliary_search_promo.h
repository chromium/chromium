// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_AUXILIARY_SEARCH_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_AUXILIARY_SEARCH_PROMO_H_

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

class PrefRegistrySimple;
class PrefService;

namespace segmentation_platform::home_modules {

// Selection logic for promo to enable auxiliary search.
class AuxiliarySearchPromo : public CardSelectionInfo {
 public:
  explicit AuxiliarySearchPromo(PrefService* profile_prefs);
  ~AuxiliarySearchPromo() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static bool IsEnabled(PrefService* profile_prefs);

  AuxiliarySearchPromo(const AuxiliarySearchPromo&) = delete;
  AuxiliarySearchPromo& operator=(const AuxiliarySearchPromo&) = delete;

  // `CardSelectionInfo` overrides.
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
  void OnShow(PrefService* profile_prefs, PrefService* local_state) override;
  void OnInteract(PrefService* profile_prefs,
                  PrefService* local_state) override;

 private:
  raw_ptr<PrefService> profile_prefs_;

  // Tracks whether the card has already recorded an impression during this
  // session so it doesn't overcount if displayed multiple times.
  bool has_been_shown_this_session_ = false;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_AUXILIARY_SEARCH_PROMO_H_
