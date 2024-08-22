// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

namespace segmentation_platform::home_modules {

HomeModulesCardRegistry::HomeModulesCardRegistry(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  CreateAllCards();
}

HomeModulesCardRegistry::~HomeModulesCardRegistry() = default;

//  static
void HomeModulesCardRegistry::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {}

void HomeModulesCardRegistry::CreateAllCards() {
  // Add all cards
  label_to_output_index_["label"] = 0;
  all_output_labels_ = {"label"};
}

base::WeakPtr<HomeModulesCardRegistry> HomeModulesCardRegistry::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace segmentation_platform::home_modules
