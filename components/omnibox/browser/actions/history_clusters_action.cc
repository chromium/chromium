// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/url_util.h"

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

namespace history_clusters {

namespace {

// A template function for recording enum metrics for shown and used journey
// chips as well as their CTR metrics.
template <class EnumT>
void RecordShownUsedEnumAndCtrMetrics(std::string_view metric_name,
                                      EnumT val,
                                      std::string_view label,
                                      bool executed) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Omnibox.ResumeJourneyShown.", metric_name}), val);
  if (executed) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Omnibox.SuggestionUsed.ResumeJourney.", metric_name}),
        val);
  }

  // Record the CTR metric.
  std::string ctr_metric_name =
      base::StrCat({"Omnibox.SuggestionUsed.ResumeJourney.", metric_name, ".",
                    label, ".CTR"});
  base::UmaHistogramBoolean(ctr_metric_name, executed);
}

}  // namespace

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

HistoryClustersAction::HistoryClustersAction(
    const std::string& query,
    const history::ClusterKeywordData& matched_keyword_data)
    : OmniboxAction(
          OmniboxAction::LabelStrings(
              IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_HINT,
              IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUGGESTION_CONTENTS,
              IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUFFIX,
              IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH),
          GetFullJourneysUrlForQuery(query)),
      matched_keyword_data_(matched_keyword_data),
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

  base::UmaHistogramBoolean("Omnibox.SuggestionUsed.ResumeJourneyCTR",
                            executed);

  // Record cluster keyword type UMA metrics.
  RecordShownUsedEnumAndCtrMetrics<
      history::ClusterKeywordData::ClusterKeywordType>(
      "ClusterKeywordType", matched_keyword_data_.type,
      matched_keyword_data_.GetKeywordTypeLabel(), executed);
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

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& HistoryClustersAction::GetVectorIcon() const {
  return omnibox::kJourneysIcon;
}
#endif

HistoryClustersAction::~HistoryClustersAction() = default;

}  // namespace history_clusters
