// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUPING_HEURISTICS_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUPING_HEURISTICS_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

// Responsible for running heuristics and providing group suggestion results.
class GroupingHeuristics {
 public:
  // Interface for each heuristic that groups tabs.
  class Heuristic {
   public:
    explicit Heuristic(GroupSuggestion::SuggestionReason reason)
        : reason_(reason) {}
    virtual ~Heuristic() = default;

    // Takes a vector of inputs, each represents data related to a single tab.
    // See kSuggestionsPredictionSchema for the available inputs and
    // AsInputContext() utility for additional data about the tab in the inputs.
    // Returns a list of floats, each value corresponds to the index of the tab
    // in the `inputs`, and represents a cluster ID where this tab should be
    // group into. A 0 cluster represents ungrouped, any non-zero value is a
    // cluster.
    // TODO(ssid): add more info about how multiple clusters, and closeness to a
    // cluster can be represented.
    virtual std::vector<float> Run(
        const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
            inputs) = 0;

    GroupSuggestion::SuggestionReason reason() const { return reason_; }

   protected:
    const GroupSuggestion::SuggestionReason reason_;
  };

  GroupingHeuristics();
  ~GroupingHeuristics();

  GroupingHeuristics(const GroupingHeuristics&) = delete;
  GroupingHeuristics& operator=(const GroupingHeuristics&) = delete;

  struct SuggestionsResult {
    SuggestionsResult();
    ~SuggestionsResult();
    SuggestionsResult(SuggestionsResult&& suggestion);
    SuggestionsResult& operator=(SuggestionsResult&& suggestion);

    // Suggestions from computation.
    std::optional<GroupSuggestions> suggestions;
    // Inputs used for suggestion computing.
    std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs;
  };

  using SuggestionResultCallback = base::OnceCallback<void(SuggestionsResult)>;

  // Runs heuristics on the `candidates` and returns a list of grouping
  // suggestions in `callback`.
  void GetSuggestions(std::vector<URLVisitAggregate> candidates,
                      SuggestionResultCallback callback);

 private:
  friend class GroupingHeuristicsTest;

  // Same as GetSuggestions(), but runs only the heuristics in the
  // `heuristics_priority` list.
  void GetSuggestions(
      std::vector<URLVisitAggregate> candidates,
      const std::vector<GroupSuggestion::SuggestionReason>& heuristics_priority,
      SuggestionResultCallback callback);

  base::flat_map<GroupSuggestion::SuggestionReason, std::unique_ptr<Heuristic>>
      heuristics_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUPING_HEURISTICS_H_
