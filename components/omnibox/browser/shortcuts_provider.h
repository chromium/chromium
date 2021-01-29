// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_PROVIDER_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
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
  explicit ShortcutsProvider(AutocompleteProviderClient* client);

  // Performs the autocompletion synchronously. Since no asynch completion is
  // performed |minimal_changes| is ignored.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  void DeleteMatch(const AutocompleteMatch& match) override;

 private:
  friend class ClassifyTest;
  friend class ShortcutsProviderExtensionTest;
  friend class ShortcutsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(ShortcutsProviderTest, CalculateScore);

  ~ShortcutsProvider() override;

  // ShortcutsBackendObserver:
  void OnShortcutsLoaded() override;

  // Performs the autocomplete matching and scoring.
  void GetMatches(const AutocompleteInput& input);

  // Returns an AutocompleteMatch corresponding to |shortcut|. Assigns it
  // |relevance| score in the process, and highlights the description and
  // contents against |input|, which should be the lower-cased version of
  // the user's input. |input| and |fixed_up_input_text| are used to decide
  // what can be inlined.
  AutocompleteMatch ShortcutToACMatch(
      const ShortcutsDatabase::Shortcut& shortcut,
      int relevance,
      const AutocompleteInput& input,
      const base::string16& fixed_up_input_text,
      const base::string16 term_string);

  // Returns iterator to first item in |shortcuts_map_| matching |keyword|.
  // Returns shortcuts_map_.end() if there are no matches.
  ShortcutsBackend::ShortcutMap::const_iterator FindFirstMatch(
      const base::string16& keyword,
      ShortcutsBackend* backend);

  int CalculateScore(const base::string16& terms,
                     const ShortcutsDatabase::Shortcut& shortcut,
                     int max_relevance);

  // The default max relevance unless overridden by a field trial.
  static const int kShortcutsProviderDefaultMaxRelevance;

  AutocompleteProviderClient* client_;
  bool initialized_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SHORTCUTS_PROVIDER_H_
