// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_ANDROID_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_ANDROID_H_

#include <string>
#include <unordered_set>

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

class PrefRegistrySimple;
class PrefService;

namespace segmentation_platform::home_modules {

// Impression counters for Android cards.
extern const char kHistorySyncPromoImpressionCounterPref[];
extern const char kTipsNotificationsPromoImpressionCounterPref[];

// Interaction flags for Android cards.
extern const char kHistorySyncPromoInteractedPref[];
extern const char kTipsNotificationsPromoInteractedPref[];

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
  void NotifyCardShown(const char* card_name) override;
  void NotifyCardInteracted(const char* card_name) override;

 private:
  // Returns true if this is the first time the card is displayed to the user in
  // the current session and the event should be recorded.
  bool ShouldNotifyCardShownPerSession(const std::string& card_name);

  // A list that includes all educational tip card types (excluding the default
  // browser promo card) that have been displayed to the user during the current
  // session.
  std::unordered_set<std::string> shown_in_current_session_;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_ANDROID_H_
