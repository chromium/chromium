// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
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
  base::ranges::transform(
      matches_begin, matches_end, relevances.begin(), [&](const auto& match) {
        return AutocompleteMatch::IsSearchType(match.type) ==
                       (filter == TopRelevanceFilter::FILTER_FOR_SEARCH_MATCHES)
                   ? match.relevance
                   : 0;
      });
  return base::ranges::max(relevances);
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

// Should be invoked after `AutocompleteResult::AttachPedalsToMatches()`.
void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    AutocompleteResult& result) {
#if BUILDFLAG(IS_IOS)
  // Compile out this method for Mobile, which doesn't omnibox actions yet.
  // This is to prevent binary size increase for no reason.
  return;
#else

  if (!service || !service->IsJourneysEnabledAndVisible()) {
    return;
  }

  if (!GetConfig().omnibox_action)
    return;

  if (result.empty())
    return;

  // If there's any action in `result`, don't add a history cluster action to
  // avoid over-crowding.
  if (!GetConfig().omnibox_action_with_pedals &&
      base::ranges::any_of(
          result, [](const auto& match) { return !match.actions.empty(); })) {
    return;
  }

  // If there's a reasonably clear navigation intent, don't distract the user
  // with the actions chip.
  if (!GetConfig().omnibox_action_on_navigation_intents &&
      IsNavigationIntent(
          TopRelevance(result.begin(), result.end(),
                       TopRelevanceFilter::FILTER_FOR_SEARCH_MATCHES),
          TopRelevance(result.begin(), result.end(),
                       TopRelevanceFilter::FILTER_FOR_NON_SEARCH_MATCHES),
          GetConfig().omnibox_action_navigation_intent_score_threshold)) {
    return;
  }

  for (auto& match : result) {
    // Skip incompatible matches (like entities) or ones with existing actions.
    // TODO(manukh): We don't use `AutocompleteMatch::IsActionCompatible()`
    //  because we're not sure if we want to show on entities or not. Once we
    //  decide, either share `IsActionCompatible()` or inline it to its
    //  remaining callsite.
    if (!match.actions.empty()) {
      continue;
    }
    if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
      continue;
    }

    if (AutocompleteMatch::IsSearchType(match.type)) {
      std::string query = base::UTF16ToUTF8(match.contents);
      std::optional<history::ClusterKeywordData> matched_keyword_data =
          service->DoesQueryMatchAnyCluster(query);
      if (matched_keyword_data) {
        match.actions.push_back(base::MakeRefCounted<HistoryClustersAction>(
            query, std::move(matched_keyword_data.value())));
      }
    }

    // Only ever attach one action (to the highest match), to not overwhelm
    // the user with multiple "Resume Journey" action buttons.
    if (!match.actions.empty()) {
      return;
    }
  }
#endif
}

}  // namespace history_clusters
