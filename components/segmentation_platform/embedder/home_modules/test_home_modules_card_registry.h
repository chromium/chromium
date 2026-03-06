// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TEST_HOME_MODULES_CARD_REGISTRY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TEST_HOME_MODULES_CARD_REGISTRY_H_

#include <memory>
#include <string_view>
#include <vector>

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

class PrefService;

namespace segmentation_platform::home_modules {

// A simple implementation of `HomeModulesCardRegistry` for use in tests.
class TestHomeModulesCardRegistry : public HomeModulesCardRegistry {
 public:
  TestHomeModulesCardRegistry(
      PrefService* profile_prefs,
      PrefService* local_state_prefs,
      std::vector<std::unique_ptr<CardSelectionInfo>> test_cards);

  ~TestHomeModulesCardRegistry() override = default;

  TestHomeModulesCardRegistry(const TestHomeModulesCardRegistry&) = delete;
  TestHomeModulesCardRegistry& operator=(const TestHomeModulesCardRegistry&) =
      delete;

  // `HomeModulesCardRegistry` overrides:
  void NotifyCardShown(std::string_view card_name) override;
  void NotifyCardInteracted(std::string_view card_name) override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TEST_HOME_MODULES_CARD_REGISTRY_H_
