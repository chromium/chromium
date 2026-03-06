// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/rank_fetcher_helper.h"

class PrefRegistrySimple;

namespace segmentation_platform::home_modules {

// Registry that manages all ephemeral cards in mobile home modules.
class HomeModulesCardRegistry : public base::SupportsUserData::Data {
 public:
  // Creates and returns specific subclass (iOS or Android) based on build
  // flags.
  static std::unique_ptr<HomeModulesCardRegistry> Create(
      PrefService* profile_prefs,
      PrefService* local_state_prefs);

  ~HomeModulesCardRegistry() override;

  HomeModulesCardRegistry(const HomeModulesCardRegistry&) = delete;
  HomeModulesCardRegistry& operator=(const HomeModulesCardRegistry&) = delete;

  // Registers profile prefs in `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers local-state prefs in `registry`.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Indicates that `card_name` was shown to the user.
  virtual void NotifyCardShown(std::string_view card_name) = 0;

  // Indicates that the user interacted with `card_name`. This could be
  // through clicking, tapping, or engaging with the card in some way.
  virtual void NotifyCardInteracted(std::string_view card_name) = 0;

  // Returns the helper responsible for fetching card ranks.
  RankFetcherHelper* get_rank_fetcher_helper() { return &rank_fetcher_helper_; }

  // Returns a list of all output labels for the registered cards.
  const std::vector<std::string>& all_output_labels() const {
    return all_output_labels_;
  }

  // Returns the total count of input signals across all cards.
  size_t all_cards_input_size() const { return all_cards_input_size_; }

  // Returns all registered cards ordered by their default priority.
  const std::vector<std::unique_ptr<CardSelectionInfo>>&
  get_all_cards_by_priority() const {
    return all_cards_by_priority_;
  }

  // Returns the mapping of each card to its input signals and output indices.
  const CardSignalMap& get_card_signal_map() const { return card_signal_map_; }

  // Returns the specific output index for a given card `label`.
  size_t get_label_index(const std::string& label) const {
    return label_to_output_index_.at(label);
  }

  base::WeakPtr<HomeModulesCardRegistry> GetWeakPtr();

 protected:
  // Protected constructor for platform-specific subclasses to initialize pref
  // services.
  HomeModulesCardRegistry(PrefService* profile_prefs,
                          PrefService* local_state_prefs);

  // Initializes state, signal maps, and label indices after subclasses populate
  // cards.
  void InitializeAfterAddingCards();

  // Registers a list of output labels and maps them to their respective output
  // indices.
  void AddCardLabels(const std::vector<std::string>& card_labels);

  // Helper responsible for fetching and managing ranking signals for the cards.
  RankFetcherHelper rank_fetcher_helper_;

  // PrefService tied to the user profile, used for tracking profile-level
  // impressions and interactions.
  const raw_ptr<PrefService> profile_prefs_;

  // PrefService tied to the local device state, used for tracking device-level
  // metrics.
  const raw_ptr<PrefService> local_state_prefs_;

  // Maps each card's specific output label to its index position in the final
  // results.
  std::map<std::string, size_t> label_to_output_index_;

  // Holds all instantiated ephemeral cards, ordered by their default fallback
  // priority.
  std::vector<std::unique_ptr<CardSelectionInfo>> all_cards_by_priority_;

  // Maps each card's name to its input signals and their corresponding index
  // orders.
  CardSignalMap card_signal_map_;

  // A comprehensive, ordered list of all output labels for every registered
  // card.
  std::vector<std::string> all_output_labels_;

  // The total combined count of input signals required across all registered
  // cards.
  size_t all_cards_input_size_{0};

 private:
  base::WeakPtrFactory<HomeModulesCardRegistry> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_H_
