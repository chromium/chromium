// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"

#if BUILDFLAG(IS_IOS)
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_ios.h"
#elif BUILDFLAG(IS_ANDROID)
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_android.h"
#endif

namespace segmentation_platform::home_modules {

// static
std::unique_ptr<HomeModulesCardRegistry> HomeModulesCardRegistry::Create(
    PrefService* profile_prefs,
    PrefService* local_state_prefs) {
#if BUILDFLAG(IS_IOS)
  return std::make_unique<HomeModulesCardRegistryIOS>(profile_prefs,
                                                      local_state_prefs);
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<HomeModulesCardRegistryAndroid>(profile_prefs,
                                                          local_state_prefs);
#else
  // Fallback for platforms that don't support home modules (e.g., Desktop).
  return nullptr;
#endif
}

// static
void HomeModulesCardRegistry::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_IOS)
  HomeModulesCardRegistryIOS::RegisterProfilePrefs(registry);
#elif BUILDFLAG(IS_ANDROID)
  HomeModulesCardRegistryAndroid::RegisterProfilePrefs(registry);
#endif
}

// static
void HomeModulesCardRegistry::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_IOS)
  HomeModulesCardRegistryIOS::RegisterLocalStatePrefs(registry);
#elif BUILDFLAG(IS_ANDROID)
  HomeModulesCardRegistryAndroid::RegisterLocalStatePrefs(registry);
#endif
}

HomeModulesCardRegistry::HomeModulesCardRegistry(PrefService* profile_prefs,
                                                 PrefService* local_state_prefs)
    : profile_prefs_(profile_prefs), local_state_prefs_(local_state_prefs) {
  CHECK(profile_prefs_);
  CHECK(local_state_prefs_);
}

HomeModulesCardRegistry::~HomeModulesCardRegistry() = default;

void HomeModulesCardRegistry::InitializeAfterAddingCards() {
  size_t input_counter = 0;
  AddCardLabels({kPlaceholderEphemeralModuleLabel});
  for (std::unique_ptr<CardSelectionInfo>& card : all_cards_by_priority_) {
    std::map<SignalKey, size_t> card_signals;
    const auto& card_inputs = card->GetInputs();
    for (const auto& key_and_input : card_inputs) {
      card_signals[key_and_input.first] = input_counter;
      input_counter++;
    }
    card_signal_map_[card->card_name()] = card_signals;

    std::vector<std::string> card_labels = card->OutputLabels();
    if (!card_labels.empty()) {
      AddCardLabels(card_labels);
    } else {
      AddCardLabels({card->card_name()});
    }
  }
  all_cards_input_size_ = input_counter;
}

void HomeModulesCardRegistry::AddCardLabels(
    const std::vector<std::string>& card_labels) {
  for (const std::string& label : card_labels) {
    CHECK(!label_to_output_index_.count(label));
    label_to_output_index_[label] = all_output_labels_.size();
    all_output_labels_.push_back(label);
  }
}

base::WeakPtr<HomeModulesCardRegistry> HomeModulesCardRegistry::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace segmentation_platform::home_modules
