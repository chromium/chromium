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
#include "components/segmentation_platform/embedder/home_modules/rank_fetcher_helper.h"

namespace segmentation_platform::home_modules {

#if BUILDFLAG(IS_ANDROID)
// Immpression counter for each card.
extern const char kDefaultBrowserPromoImpressionCounterPref[];
extern const char kTabGroupPromoImpressionCounterPref[];
extern const char kTabGroupSyncPromoImpressionCounterPref[];
extern const char kQuickDeletePromoImpressionCounterPref[];
extern const char kAuxiliarySearchPromoImpressionCounterPref[];
extern const char kHistorySyncPromoImpressionCounterPref[];

// Interaction flag for each card.
extern const char kDefaultBrowserPromoInteractedPref[];
extern const char kTabGroupPromoInteractedPref[];
extern const char kTabGroupSyncPromoInteractedPref[];
extern const char kQuickDeletePromoInteractedPref[];
extern const char kAuxiliarySearchPromoInteractedPref[];
extern const char kHistorySyncPromoInteractedPref[];
#endif

// Registry that manages all ephemeral cards in mobile home modules.
class HomeModulesCardRegistry : public base::SupportsUserData::Data {
 public:
  explicit HomeModulesCardRegistry(PrefService* profile_prefs);
  // For testing.
  HomeModulesCardRegistry(
      PrefService* profile_prefs,
      std::vector<std::unique_ptr<CardSelectionInfo>> cards);
  ~HomeModulesCardRegistry() override;

  HomeModulesCardRegistry(const HomeModulesCardRegistry&) = delete;
  HomeModulesCardRegistry& operator=(const HomeModulesCardRegistry&) = delete;

  // Registers all the profile prefs needed for the ephemeral cards system.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns `true` if the given `label` corresponds to any of the Tips
  // ephemeral module classes.
  static bool IsEphemeralTipsModuleLabel(std::string_view label);

  // Indicates that `card_name` was shown to the user.
  void NotifyCardShown(const char* card_name);

  // Indicates that the user interacted with `card_name`. This could be
  // through clicking, tapping, or engaging with the card in some way.
  void NotifyCardInteracted(const char* card_name);

  RankFetcherHelper* get_rank_fecther_helper() { return &rank_fecther_helper_; }

  const std::vector<std::string>& all_output_labels() const {
    return all_output_labels_;
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

#if BUILDFLAG(IS_ANDROID)
  // Returns true if this is the first time the card is displayed to the user in
  // the current session and the event should be recorded.
  bool ShouldNotifyCardShownPerSession(const std::string& card_name);
#endif

 private:
  // Populats `all_cards_by_priority_`.
  void CreateAllCards();

  // Initializes the registry after all cards are added.
  void InitializeAfterAddingCards();

  // Adds `card_labels` to the registry.
  void AddCardLabels(const std::vector<std::string>& card_labels);

  RankFetcherHelper rank_fecther_helper_;

  const raw_ptr<PrefService> profile_prefs_;

  // Maps a card label to its output index order.
  std::map<std::string, size_t> label_to_output_index_;

  // List of cards by their default priority.
  std::vector<std::unique_ptr<CardSelectionInfo>> all_cards_by_priority_;

  // Holds a map of each card to a map of its input signals to their overall
  // index order of the returned fetch.
  CardSignalMap card_signal_map_;

  // All the output labels for all the cards registered by this class.
  std::vector<std::string> all_output_labels_;

  // The total count of the inputs of all cards.
  size_t all_cards_input_size_{0};

#if BUILDFLAG(IS_ANDROID)
  // A list includes all educational tip card types (excluding the default
  // browser promo card) that have been displayed to the user during the current
  // session.
  std::unordered_set<std::string> shown_in_current_session_;
#endif

  base::WeakPtrFactory<HomeModulesCardRegistry> weak_ptr_factory_{this};

  // Returns the list of card names configured via the
  // "names_of_ephemeral_cards_to_show" feature param. The param is expected to
  // be a comma-separated string (e.g.,
  // "TabGroupPromo,TabGroupSyncPromo,QuickDeletePromo").
  std::vector<std::string> GetEnabledCardList();
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_HOME_MODULES_CARD_REGISTRY_H_
