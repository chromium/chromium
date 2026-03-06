// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_ANDROID_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_ANDROID_H_

#include <string>
#include <string_view>
#include <unordered_set>

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

class PrefRegistrySimple;
class PrefService;

namespace segmentation_platform::home_modules {

// The Android-specific implementation of the HomeModulesCardRegistry.
class HomeModulesCardRegistryAndroid : public HomeModulesCardRegistry {
 public:
  HomeModulesCardRegistryAndroid(PrefService* profile_prefs,
                                 PrefService* local_state_prefs);
  ~HomeModulesCardRegistryAndroid() override;

  HomeModulesCardRegistryAndroid(const HomeModulesCardRegistryAndroid&) =
      delete;
  HomeModulesCardRegistryAndroid& operator=(
      const HomeModulesCardRegistryAndroid&) = delete;

  // Registers all the profile prefs needed for the Android ephemeral cards.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers all the local state prefs needed for the Android ephemeral cards.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // `HomeModulesCardRegistry` overrides:
  void NotifyCardShown(std::string_view card_name) override;
  void NotifyCardInteracted(std::string_view card_name) override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_ANDROID_H_
