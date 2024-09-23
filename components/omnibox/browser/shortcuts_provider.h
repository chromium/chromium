// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_PROVIDER_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/shortcuts_backend.h"

class AutocompleteProviderClient;
class ShortcutsProviderTest;

// Provider of recently autocompleted links. Provides autocomplete suggestions
// from previously selected suggestions. The more often a user selects a
// suggestion for a given search term the higher will be that suggestion's
// ranking for future uses of that search term.
class ShortcutsProvider : public AutocompleteProvider,
                          public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  // ShortcutMatch holds sufficient information about a single match from the
  // shortcut database to allow for destination deduping and relevance sorting.
  // After those stages the top matches are converted to the more heavyweight
  // AutocompleteMatch struct.  Avoiding constructing the larger struct for
  // every such match can save significant time when there are many shortcut
  // matches to process.
  // TODO(manukh): We should probably merge `ShortcutMatch` into
  //   `ShortcutsDatabase::Shortcut`. There's a 4-deep hierarchy of structs:
  //   - `AutocompleteMatch` are created from `ShortcutMatch`es
  //   - `ShortcutMatch`es own `ShortcutsDatabase::Shortcut`s
  //   - `ShortcutsDatabase::Shortcut`s own
  //     `ShortcutsDatabase::Shortcut::MatchCore`s
  struct ShortcutMatch {
    ShortcutMatch(int relevance,
                  int aggregate_number_of_hits,
                  base::Time most_recent_access_time,
                  size_t shortest_text_length,
                  const GURL& stripped_destination_url,
                  const ShortcutsDatabase::Shortcut* shortcut);

    ShortcutMatch(const ShortcutMatch& other);
    ShortcutMatch& operator=(const ShortcutMatch& other);

    int relevance;
    // The sum of `number_of_hits` of all deduped shortcuts.
    int aggregate_number_of_hits;
    base::Time most_recent_access_time;
    size_t shortest_text_length;
    GURL stripped_destination_url;
    raw_ptr<const ShortcutsDatabase::Shortcut> shortcut;
    std::u16string contents;
    AutocompleteMatch::Type type;
  };

  explicit ShortcutsProvider(AutocompleteProviderClient* client);

  // Performs the autocompletion synchronously. Since no asynch completion is
  // performed |minimal_changes| is ignored.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  void DeleteMatch(const AutocompleteMatch& match) override;

 private:
  friend class ClassifyTest;
  friend class ShortcutsProviderExtensionTest;
  friend class ShortcutsProviderTest;

  ~ShortcutsProvider() override;

  // ShortcutsBackendObserver:
  void OnShortcutsLoaded() override;

  // Performs the autocomplete matching and scoring. Populates matches results
  // with scoring signals for ML models if enabled. Only populates signals for
  // ULR matches for now.
  void DoAutocomplete(const AutocompleteInput& input,
                      bool populate_scoring_signals);

  // Creates a shortcut match by aggregating the scoring factors from a vector
  // of `shortcuts`. Specifically:
  // - Considers the shortest shortcut when computing fraction typed.
  // - Considers the most recent shortcut when considering last visit.
  // - Considers the sum of `number_of_hits`.
  // - Considers the shortest contents when picking a shortcut.
  // Returns the shortcut match with the aggregated score.
  ShortcutMatch CreateScoredShortcutMatch(
      size_t input_length,
      const GURL& stripped_destination_url,
      const std::vector<const ShortcutsDatabase::Shortcut*>& shortcuts,
      int max_relevance);

  // Returns an AutocompleteMatch corresponding to `shortcut_match`. Highlights
  // the description and contents against `input`, which should be the
  // normalized version of the user's input. `input` and `fixed_up_input_text`
  // are used to decide what can be inlined.
  AutocompleteMatch ShortcutMatchToACMatch(
      const ShortcutMatch& shortcut_match,
      int relevance,
      const AutocompleteInput& input,
      const std::u16string& fixed_up_input_text,
      const std::u16string lower_input);

  // Returns iterator to first item in |shortcuts_map_| matching |keyword|.
  // Returns shortcuts_map_.end() if there are no matches.
  ShortcutsBackend::ShortcutMap::const_iterator FindFirstMatch(
      const std::u16string& keyword,
      ShortcutsBackend* backend);

  // The default max relevance unless overridden by a field trial.
  static const int kShortcutsProviderDefaultMaxRelevance;

  raw_ptr<AutocompleteProviderClient> client_ = nullptr;
  scoped_refptr<ShortcutsBackend> backend_;
  bool initialized_{};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_PROVIDER_H_
