// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace history_clusters {

int TopRelevance(std::vector<AutocompleteMatch>::const_iterator matches_begin,
                 std::vector<AutocompleteMatch>::const_iterator matches_end,
                 TopRelevanceFilter filter) {
  if (matches_begin == matches_end)
    return 0;
  std::vector<int> relevances(matches_end - matches_begin);
  std::ranges::transform(
      matches_begin, matches_end, relevances.begin(), [&](const auto& match) {
        return AutocompleteMatch::IsSearchType(match.type) ==
                       (filter == TopRelevanceFilter::FILTER_FOR_SEARCH_MATCHES)
                   ? match.relevance
                   : 0;
      });
  return std::ranges::max(relevances);
}

bool IsNavigationIntent(int top_search_relevance,
                        int top_navigation_relevance,
                        int navigation_intent_score_threshold) {
  return top_navigation_relevance > top_search_relevance &&
         top_navigation_relevance > navigation_intent_score_threshold;
}

GURL GetFullJourneysUrlForQuery(const std::string& query) {
  return net::AppendOrReplaceQueryParameter(
      GURL(GetChromeUIHistoryClustersURL()), "q", query);
}

HistoryClustersAction::HistoryClustersAction(const std::string& query)
    : OmniboxAction(OmniboxAction::LabelStrings(),
                    GetFullJourneysUrlForQuery(query)),
      query_(query) {}

void HistoryClustersAction::RecordActionShown(size_t position,
                                              bool executed) const {
  base::UmaHistogramExactLinear(
      "Omnibox.ResumeJourneyShown", position,
      AutocompleteResult::kMaxAutocompletePositionValue);

  if (executed) {
    base::UmaHistogramExactLinear(
        "Omnibox.SuggestionUsed.ResumeJourney", position,
        AutocompleteResult::kMaxAutocompletePositionValue);
  }
}

void HistoryClustersAction::Execute(ExecutionContext& context) const {
  if (context.client_->OpenJourneys(query_)) {
    // If the client opens Journeys in the Side Panel, we are done.
    return;
  }
  // Otherwise call the superclass, which will open the WebUI URL.
  OmniboxAction::Execute(context);
}

OmniboxActionId HistoryClustersAction::ActionId() const {
  return OmniboxActionId::HISTORY_CLUSTERS;
}

HistoryClustersAction::~HistoryClustersAction() = default;

}  // namespace history_clusters
