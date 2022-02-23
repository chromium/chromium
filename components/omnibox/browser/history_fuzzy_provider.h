// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_

#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_provider.h"

// This class is an autocomplete provider which provides URL results from
// history for inputs that may match inexactly.
// The mechanism that makes it "fuzzy" is a trie search that tolerates errors
// and produces corrected inputs which can then be autocompleted as normal.
// Relevance penalties are applied for corrections as errors reduce confidence.
// The trie is built from history URL text and any text that can be formed
// by tracing a path from the root is said to be "on trie" while any text
// that escapes the trie with a character not present is said to be "off trie".
// If the autocomplete input is fully on trie, no suggestions are provided
// because exact matching should already provide the best results. If the
// autocomplete input is off trie, corrections of bounded size are produced
// to get the input back on trie, and these should then be eligible for
// autocompletion.
// The data structure could contain anything helpful, including ways to mark
// terminal nodes (signaling a path as a complete string). A relevance score
// could serve this purpose and make full independent autocompletion possible,
// but efficiency is a top concern so node size is minimized, just enough to
// get inputs back on track, not to replicate the full completion and scoring
// of other autocomplete providers.
class HistoryFuzzyProvider : public HistoryProvider {
 public:
  explicit HistoryFuzzyProvider(AutocompleteProviderClient* client);
  HistoryFuzzyProvider(const HistoryFuzzyProvider&) = delete;
  HistoryFuzzyProvider& operator=(const HistoryFuzzyProvider&) = delete;

  // AutocompleteProvider. `minimal_changes` is ignored since there is no async
  // completion performed.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;

 private:
  ~HistoryFuzzyProvider() override;

  // Performs the autocomplete matching and scoring.
  void DoAutocomplete();

  // Adds one match for the given corrected `text`.
  void AddMatchForText(std::u16string text);

  AutocompleteInput autocomplete_input_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_
