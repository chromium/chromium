// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_

#include <set>

#include "components/omnibox/browser/autocomplete_match.h"

// Tracks the features that trigger during an omnibox session and records them
// to the logs. This is used for counterfactual slicing metrics by feature.
class OmniboxTriggeredFeatureService {
 public:
  // The list of features used for counterfactual slicing.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. When adding an entry here, a
  // corresponding entry should be added in the UMA histograms.
  enum class Feature {
    kRichAutocompletion = 0,
    kBookmarkPaths = 1,
    kShortBookmarkSuggestionsByTotalInputLength = 2,
    kFuzzyUrlSuggestions = 3,
    kHistoryClusterSuggestion = 4,
    kMaxValue = kHistoryClusterSuggestion,
  };
  using Features = std::set<Feature>;

  OmniboxTriggeredFeatureService();
  ~OmniboxTriggeredFeatureService();

  // Records `features_` for this session to `feature_triggered_in_session`.
  // Records UMA histograms for any non-omnibox-event-protobuf features (i.e.
  // `rich_autocompletion_types_`).
  void RecordToLogs(Features* feature_triggered_in_session) const;

  // Invoked to indicate `feature` was triggered.
  void FeatureTriggered(Feature feature);

  // Invoked to indicate `rich_autocompletion_type` was triggered. Multiple
  // types can be triggered in a session. Does not automatically trigger
  // `kRichAutocompletion`.
  void RichAutocompletionTypeTriggered(
      AutocompleteMatch::RichAutocompletionType rich_autocompletion_type);

  // Invoked when a new omnibox session starts. Clears `features_` and
  // `rich_autocompletion_types_`.
  void ResetSession();

 private:
  // The set of features triggered in the current omnibox session via
  // `FeatureTriggered()`.
  std::set<Feature> features_;

  // The set of rich autocompletion types triggered in the current omnibox
  // session via `RichAutocompletionTypeTriggered()`.
  std::set<AutocompleteMatch::RichAutocompletionType>
      rich_autocompletion_types_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_TRIGGERED_FEATURE_SERVICE_H_
