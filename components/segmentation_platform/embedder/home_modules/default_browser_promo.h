// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_H_

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

class PrefRegistrySimple;
class PrefService;

namespace segmentation_platform::home_modules {

// 'DefaultBrowserPromo' is a class that represents an ephemeral home module,
// specifically designed for the default browser promo card found in the
// educational tip section on chrome android. It is responsible for determining
// whether the module should be shown to the user based on the enable signal and
// the user's interaction history.
class DefaultBrowserPromo : public CardSelectionInfo {
 public:
  explicit DefaultBrowserPromo(PrefService* profile_prefs);
  ~DefaultBrowserPromo() override = default;

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
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_DEFAULT_BROWSER_PROMO_H_
