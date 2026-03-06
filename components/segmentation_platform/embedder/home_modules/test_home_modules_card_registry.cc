// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/test_home_modules_card_registry.h"

#include "components/prefs/pref_service.h"

namespace segmentation_platform::home_modules {

TestHomeModulesCardRegistry::TestHomeModulesCardRegistry(
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    std::vector<std::unique_ptr<CardSelectionInfo>> test_cards)
    : HomeModulesCardRegistry(profile_prefs, local_state_prefs) {
  all_cards_by_priority_ = std::move(test_cards);
  InitializeAfterAddingCards();
}

void TestHomeModulesCardRegistry::NotifyCardShown(std::string_view card_name) {}

void TestHomeModulesCardRegistry::NotifyCardInteracted(
    std::string_view card_name) {}

}  // namespace segmentation_platform::home_modules
