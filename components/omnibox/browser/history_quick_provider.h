// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_QUICK_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_QUICK_PROVIDER_H_

#include <optional>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/in_memory_url_index.h"

struct ScoredHistoryMatch;

// This class is an autocomplete provider (a pseudo-internal component of
// the history system) which quickly (and synchronously) provides matching
// results from recently or frequently visited sites in the profile's
// history.
class HistoryQuickProvider : public HistoryProvider {
 public:
  explicit HistoryQuickProvider(AutocompleteProviderClient* client);
  HistoryQuickProvider(const HistoryQuickProvider&) = delete;
  HistoryQuickProvider& operator=(const HistoryQuickProvider&) = delete;

  // AutocompleteProvider. |minimal_changes| is ignored since there is no asynch
  // completion performed.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;

  // Disable this provider. For unit testing purposes only. This is required
  // because this provider is closely associated with the HistoryURLProvider
  // and in order to properly test the latter the HistoryQuickProvider must
  // be disabled.
  // TODO(mrossetti): Eliminate this once the HUP has been refactored.
  static void set_disabled(bool disabled) { disabled_ = disabled; }

 private:
  friend class HistoryQuickProviderTest;

  ~HistoryQuickProvider() override;

  // Performs the autocomplete matching and scoring.
  void DoAutocomplete();

  // Predicts if there may be a URL-what-you-typed match. If so, returns a
  // prediction of its score, which HQP suggestions shouldn't exceed. Returns
  // `nullopt` otherwise. The goal is for URL-what-you-typed matches for visited
  // URLs to beat out any longer URLs, no matter how frequently they're visited.
  std::optional<int> MaxMatchScore();

  // Creates an AutocompleteMatch from |history_match|, assigning it
  // the score |score|.
  AutocompleteMatch QuickMatchToACMatch(const ScoredHistoryMatch& history_match,
                                        int score);

  AutocompleteInput autocomplete_input_;
  raw_ptr<InMemoryURLIndex> in_memory_url_index_;  // Not owned by this class.
  raw_ptr<const TemplateURL> starter_pack_engine_;

  // This provider is disabled when true.
  static bool disabled_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_QUICK_PROVIDER_H_
