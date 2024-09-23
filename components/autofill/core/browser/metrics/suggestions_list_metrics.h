// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_

#include <cstddef>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace autofill {
enum class FillingProduct;

namespace autofill_metrics {

// Stores information on how the new ranking algorithm experiment ranks
// suggestions compared to the legacy ranking algorithm.
struct SuggestionRankingContext {
  SuggestionRankingContext();
  SuggestionRankingContext(const SuggestionRankingContext&);
  SuggestionRankingContext& operator=(const SuggestionRankingContext&);
  ~SuggestionRankingContext();

  // Represents the difference in ranking between the new and old algorithms. In
  // these cases, "higher" or "lower" defines the new algorithm's placement
  // compared to where the suggestion was in the legacy algorithm.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RelativePosition {
    kRankedHigher = 0,
    kRankedLower = 1,
    kRankedSame = 2,
    kMaxValue = kRankedSame,
  };

  // Given `legacy_index` and `new_index`, returns the appropriate
  // RelativePosition enum.
  static RelativePosition GetRelativePositionEnum(size_t legacy_index,
                                                  size_t new_index);

  // Checks if the ranking of all suggestions are the same or different between
  // the new and legacy algorithm.
  bool RankingsAreDifferent() const;

  // Contains a mapping of a suggestion to its RelativePosition enum.
  base::flat_map<Suggestion::Guid, RelativePosition>
      suggestion_rankings_difference_map;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Used by LogAutofillShowCardsFromGoogleAccountButtonEventMetric().
enum class ShowCardsFromGoogleAccountButtonEvent {
  // 'Show Cards from Google Account' button appeared.
  kButtonAppeared = 0,
  // 'Show Cards from Google Account' button appeared. Logged once per page
  // load.
  kButtonAppearedOnce = 1,
  // 'Show Cards from Google Account' button clicked.
  kButtonClicked = 2,
  kMaxValue = kButtonClicked,
};

// Log the number of Autofill suggestions for the given
// `filling_product`presented to the user when displaying the autofill popup.
void LogSuggestionsCount(size_t num_suggestions,
                         FillingProduct filling_product);

// Log the index of the selected Autofill suggestion in the popup.
void LogSuggestionAcceptedIndex(int index,
                                FillingProduct filling_product,
                                bool off_the_record);

// Logs the 'Show cards from your Google Account" button events.
void LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
    ShowCardsFromGoogleAccountButtonEvent event);

// Logs a `ranking_difference` for a recently-selected Autofill suggestion
// selection, recording if the suggestion was ranked higher or lower in the new
// ranking algorithm compared to the legacy ranking algorithm.
void LogAutofillRankingSuggestionDifference(
    SuggestionRankingContext::RelativePosition ranking_difference);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
