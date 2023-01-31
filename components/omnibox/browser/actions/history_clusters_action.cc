// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
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
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/omnibox/browser/actions/omnibox_pedal_jni_wrapper.h"
#include "url/android/gurl_android.h"
#endif

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

namespace history_clusters {

namespace {

// A template function for recording enum metrics for shown and used journey
// chips as well as their CTR metrics.
template <class EnumT>
void RecordShownUsedEnumAndCtrMetrics(const std::string& metric_name,
                                      EnumT val,
                                      const std::string& label,
                                      bool executed) {
  base::UmaHistogramEnumeration("Omnibox.ResumeJourneyShown." + metric_name,
                                val);
  if (executed) {
    base::UmaHistogramEnumeration(
        "Omnibox.SuggestionUsed.ResumeJourney." + metric_name, val);
  }

  // Record the CTR metric.
  std::string ctr_metric_name =
      base::StringPrintf("Omnibox.SuggestionUsed.ResumeJourney.%s.%s.CTR",
                         metric_name.c_str(), label.c_str());
  base::UmaHistogramBoolean(ctr_metric_name, executed);
}

// Multiplies a keyword score by 100, and converts it to int.
int TransformKeywordScoreForUma(float keyword_score) {
  return static_cast<int>(keyword_score * 100);
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
  return net::AppendOrReplaceQueryParameter(GURL(kChromeUIHistoryClustersURL),
                                            "q", query);
}

HistoryClustersAction::HistoryClustersAction(
    const std::string& query,
    const history::ClusterKeywordData& matched_keyword_data,
    bool takes_over_match)
    : OmniboxAction(
          OmniboxAction::LabelStrings(
              IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_HINT,
              IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUGGESTION_CONTENTS,
              IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUFFIX,
              IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH),
          GetFullJourneysUrlForQuery(query),
          takes_over_match),
      matched_keyword_data_(matched_keyword_data),
      query_(query) {
#if BUILDFLAG(IS_ANDROID)
    CreateOrUpdateJavaObject(query);
#endif
}

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

  // Record cluster keyword score UMA metrics.
  base::UmaHistogramCounts1000(
      "Omnibox.ResumeJourneyShown.ClusterKeywordScore",
      TransformKeywordScoreForUma(matched_keyword_data_.score));
  if (executed) {
    base::UmaHistogramCounts1000(
        "Omnibox.SuggestionUsed.ResumeJourney.ClusterKeywordScore",
        TransformKeywordScoreForUma(matched_keyword_data_.score));
  }

  // Record cluster keyword type UMA metrics.
  RecordShownUsedEnumAndCtrMetrics<
      history::ClusterKeywordData::ClusterKeywordType>(
      "ClusterKeywordType", matched_keyword_data_.type,
      matched_keyword_data_.GetKeywordTypeLabel(), executed);

  // Record entity collection UMA metrics.
  if (matched_keyword_data_.entity_collections.empty()) {
    return;
  }
  const auto& collection_str = matched_keyword_data_.entity_collections.front();
  const optimization_guide::PageEntityCollection collection =
      optimization_guide::GetPageEntityCollectionForString(collection_str);
  const auto collection_label =
      optimization_guide::GetPageEntityCollectionLabel(collection_str);
  RecordShownUsedEnumAndCtrMetrics<optimization_guide::PageEntityCollection>(
      "PageEntityCollection", collection, collection_label, executed);
}

void HistoryClustersAction::Execute(ExecutionContext& context) const {
  if (context.client_->OpenJourneys(query_)) {
    // If the client opens Journeys in the Side Panel, we are done.
    return;
  }
  // Otherwise call the superclass, which will open the WebUI URL.
  OmniboxAction::Execute(context);
}

int32_t HistoryClustersAction::GetID() const {
  return static_cast<int32_t>(OmniboxActionId::HISTORY_CLUSTERS);
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& HistoryClustersAction::GetVectorIcon() const {
  return omnibox::kJourneysIcon;
}
#endif

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaGlobalRef<jobject>
HistoryClustersAction::GetJavaObject() const {
  return j_omnibox_action_;
}

void HistoryClustersAction::CreateOrUpdateJavaObject(const std::string& query) {
  j_omnibox_action_.Reset(BuildHistoryClustersAction(
      GetID(), strings_.hint, strings_.suggestion_contents,
      strings_.accessibility_suffix, strings_.accessibility_hint, url_, query));
}
#endif

HistoryClustersAction::~HistoryClustersAction() = default;

// Should be invoked after `AutocompleteResult::AttachPedalsToMatches()`.
void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    PrefService* prefs,
    AutocompleteResult& result) {
#if BUILDFLAG(IS_IOS)
  // Compile out this method for Mobile, which doesn't omnibox actions yet.
  // This is to prevent binary size increase for no reason.
  return;
#else

  if (!IsJourneysEnabledInOmnibox(service, prefs))
    return;

  if (!GetConfig().omnibox_action)
    return;

  if (result.empty())
    return;

  // If there's any visible action in `result`, don't add a history cluster
  // action to avoid over-crowding.
  if (!GetConfig().omnibox_action_with_pedals &&
      base::ranges::any_of(result, [](const auto& match) {
        return base::ranges::any_of(match.actions, [](const auto& action) {
          return !action->TakesOverMatch();
        });
      })) {
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
      absl::optional<history::ClusterKeywordData> matched_keyword_data =
          service->DoesQueryMatchAnyCluster(query);
      if (matched_keyword_data) {
        match.actions.push_back(base::MakeRefCounted<HistoryClustersAction>(
            query, std::move(matched_keyword_data.value()),
            /*takes_over_match=*/false));
      }
    } else if (GetConfig().omnibox_action_on_urls) {
      // We do the URL stripping here, because we need it to both execute the
      // query, as well as to feed it into the action chip so the chip navigates
      // to the right place (with the query pre-populated).
      std::string url_keyword =
          history_clusters::ComputeURLKeywordForLookup(match.destination_url);
      if (service->DoesURLMatchAnyCluster(url_keyword)) {
        match.actions.push_back(base::MakeRefCounted<HistoryClustersAction>(
            url_keyword, history::ClusterKeywordData(),
            /*takes_over_match=*/false));
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
