// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_IOS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_IOS_H_

#include <string_view>

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

class PrefRegistrySimple;
class PrefService;

namespace segmentation_platform::home_modules {

// The iOS-specific implementation of the HomeModulesCardRegistry.
class HomeModulesCardRegistryIOS : public HomeModulesCardRegistry {
 public:
  HomeModulesCardRegistryIOS(PrefService* profile_prefs,
                             PrefService* local_state_prefs);
  ~HomeModulesCardRegistryIOS() override;

  HomeModulesCardRegistryIOS(const HomeModulesCardRegistryIOS&) = delete;
  HomeModulesCardRegistryIOS& operator=(const HomeModulesCardRegistryIOS&) =
      delete;

  // Registers all the profile prefs needed for the iOS ephemeral cards.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers all the local state prefs needed for the iOS ephemeral cards.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Returns `true` if the given `label` corresponds to any of the Tips
  // ephemeral module classes on iOS.
  static bool IsEphemeralTipsModuleLabel(std::string_view label);

  // `HomeModulesCardRegistry` overrides:
  void NotifyCardShown(std::string_view card_name) override;
  void NotifyCardInteracted(std::string_view card_name) override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_IOS_H_
