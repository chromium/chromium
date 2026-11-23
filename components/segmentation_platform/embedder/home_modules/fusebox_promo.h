// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_FUSEBOX_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_FUSEBOX_PROMO_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

class PrefRegistrySimple;
class PrefService;

namespace segmentation_platform::home_modules {

// 'FuseboxPromo' is a class that represents an ephemeral home module,
// specifically designed for the Fusebox promo card found in the educational
// tip section on Chrome Android. It is responsible for determining whether the
// module should be shown to the user based on organic Fusebox/AI Mode usage and
// the user's interaction history.
class FuseboxPromo : public CardSelectionInfo {
 public:
  explicit FuseboxPromo(PrefService* profile_prefs);
  ~FuseboxPromo() override = default;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static bool IsEnabled(PrefService* profile_prefs);

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

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_FUSEBOX_PROMO_H_
