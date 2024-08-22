// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"

namespace segmentation_platform::home_modules {

// Registry that manages all ephemeral cards in mobile home modules.
class HomeModulesCardRegistry : public base::SupportsUserData::Data {
 public:
  explicit HomeModulesCardRegistry(PrefService* profile_prefs);
  ~HomeModulesCardRegistry() override;

  HomeModulesCardRegistry(const HomeModulesCardRegistry&) = delete;
  HomeModulesCardRegistry& operator=(const HomeModulesCardRegistry&) = delete;

  // Registers all the profile prefs needed for the ephemeral cards system.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // TODO(thegreenfrog): This class needs to be cleaned up with appropriate
  // methods.

  const std::vector<std::string>& all_output_labels() const {
    return all_output_labels_;
  }

  const std::vector<std::string>& all_card_ouput_labels() const {
    return all_cards_ouptut_labels_;
  }

  size_t all_cards_input_size() const { return all_cards_input_size_; }

  const std::vector<std::unique_ptr<CardSelectionInfo>>&
  get_all_cards_by_priority() const {
    return all_cards_by_priority_;
  }

  const CardSignalMap& get_card_signal_map() const { return card_signal_map_; }

  size_t get_label_index(const std::string& label) const {
    return label_to_output_index_.at(label);
  }

  base::WeakPtr<HomeModulesCardRegistry> GetWeakPtr();

 private:
  // Populats `all_cards_by_priority_`.
  void CreateAllCards();

  const raw_ptr<PrefService> profile_prefs_;

  std::map<std::string, size_t> label_to_output_index_;
  std::vector<std::unique_ptr<CardSelectionInfo>> all_cards_by_priority_;
  std::vector<std::string> all_cards_ouptut_labels_;
  CardSignalMap card_signal_map_;
  std::vector<std::string> all_output_labels_;
  size_t all_cards_input_size_{0};

  base::WeakPtrFactory<HomeModulesCardRegistry> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_H_
