// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_cluster_provider.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/url_constants.h"
#include "components/omnibox/browser/actions/history_clusters_action.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

HistoryClusterProvider::HistoryClusterProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener,
    AutocompleteProvider* search_provider,
    AutocompleteProvider* history_url_provider,
    AutocompleteProvider* history_quick_provider)
    : AutocompleteProvider(AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER),
      client_(client),
      search_provider_(search_provider),
      history_url_provider_(history_url_provider),
      history_quick_provider_(history_quick_provider) {
  DCHECK(search_provider_);
  DCHECK(history_url_provider_);
  DCHECK(history_quick_provider_);
  AddListener(listener);
  search_provider_->AddListener(this);
  history_url_provider_->AddListener(this);
  history_quick_provider_->AddListener(this);
}

// static
void HistoryClusterProvider::CompleteHistoryClustersMatch(
    const std::string& matching_text,
    history::ClusterKeywordData matched_keyword_data,
    AutocompleteMatch* match) {
  DCHECK(match);

  // It's fine to unconditionally attach this takeover action, as the action
  // itself checks the flag to redirect the user to either the Side Panel or
  // the traditional History/Journeys WebUI. As a side effect, it will also
  // record the action-centric metrics.
  DCHECK(match->actions.empty());
  match->takeover_action =
      base::MakeRefCounted<history_clusters::HistoryClustersAction>(
          matching_text, std::move(matched_keyword_data));
}

void HistoryClusterProvider::Start(const AutocompleteInput& input,
                                   bool minimal_changes) {
  Stop(true, false);

  if (input.omit_asynchronous_matches())
    return;

  if (!client_->GetHistoryClustersService() ||
      !client_->GetHistoryClustersService()->IsJourneysEnabledAndVisible()) {
    return;
  }

  if (!history_clusters::GetConfig().omnibox_history_cluster_provider)
    return;

  done_ = false;
  input_ = input;

  if (AllProvidersDone())
    CreateMatches();
}

void HistoryClusterProvider::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  if (done_ || !AllProvidersDone())
    return;
  NotifyListeners(CreateMatches());
}

bool HistoryClusterProvider::AllProvidersDone() {
  return search_provider_->done() && history_url_provider_->done() &&
         history_quick_provider_->done();
}

bool HistoryClusterProvider::CreateMatches() {
  done_ = true;

  // If there's a reasonably clear navigation intent, don't distract the user
  // with a history cluster suggestion.
  if (!history_clusters::GetConfig()
           .omnibox_history_cluster_provider_on_navigation_intents) {
    // Helper to get the top relevance score looking at both providers.
    const auto top_relevance =
        [&](history_clusters::TopRelevanceFilter filter) {
          return std::max(
              {history_clusters::TopRelevance(
                   search_provider_->matches().begin(),
                   search_provider_->matches().end(), filter),
               history_clusters::TopRelevance(
                   history_url_provider_->matches().begin(),
                   history_url_provider_->matches().end(), filter),
               history_clusters::TopRelevance(
                   history_quick_provider_->matches().begin(),
                   history_quick_provider_->matches().end(), filter)});
        };
    if (history_clusters::IsNavigationIntent(
            top_relevance(history_clusters::TopRelevanceFilter::
                              FILTER_FOR_SEARCH_MATCHES),
            top_relevance(history_clusters::TopRelevanceFilter::
                              FILTER_FOR_NON_SEARCH_MATCHES),
            history_clusters::GetConfig()
                .omnibox_history_cluster_provider_navigation_intent_score_threshold)) {
      return false;
    }
  }

  // Iterate search matches in their current order. This is usually highest to
  // lowest relevance with an exception for search-what-you-typed search
  // suggestions being ordered before others.
  for (const auto& search_match : search_provider_->matches()) {
    auto matched_keyword_data =
        client_->GetHistoryClustersService()->DoesQueryMatchAnyCluster(
            base::UTF16ToUTF8(search_match.contents));
    if (matched_keyword_data) {
      client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
          metrics::OmniboxEventProto_Feature_HISTORY_CLUSTER_SUGGESTION);
      if (!history_clusters::GetConfig()
               .omnibox_history_cluster_provider_counterfactual) {
        matches_.push_back(
            CreateMatch(search_match, std::move(matched_keyword_data.value())));
      }
      return true;
    }
  }
  return false;
}

AutocompleteMatch HistoryClusterProvider::CreateMatch(
    const AutocompleteMatch& search_match,
    history::ClusterKeywordData matched_keyword_data) {
  AutocompleteMatch match;
  match.provider = this;
  match.type = AutocompleteMatch::Type::HISTORY_CLUSTER;

  match.relevance =
      history_clusters::GetConfig()
              .omnibox_history_cluster_provider_inherit_search_match_score
          ? search_match.relevance - 1
          : history_clusters::GetConfig()
                .omnibox_history_cluster_provider_score;

  const auto& text = search_match.contents;

  match.destination_url = GURL(
      base::UTF8ToUTF16(history_clusters::GetChromeUIHistoryClustersURL() +
                        base::StringPrintf("?q=%s", base::EscapeQueryParamValue(
                                                        base::UTF16ToUTF8(text),
                                                        /*use_plus=*/false)
                                                        .c_str())));

  match.fill_into_edit = text;

  match.description = text;
  match.description_class = ClassifyTermMatches(
      FindTermMatches(input_.text(), text), text.length(),
      ACMatchClassification::MATCH, ACMatchClassification::NONE);

  match.contents =
      l10n_util::GetStringUTF16(IDS_OMNIBOX_HISTORY_CLUSTERS_SEARCH_HINT);
  match.contents_class = {{0, ACMatchClassification::DIM}};

  CompleteHistoryClustersMatch(base::UTF16ToUTF8(text),
                               std::move(matched_keyword_data), &match);

  return match;
}
