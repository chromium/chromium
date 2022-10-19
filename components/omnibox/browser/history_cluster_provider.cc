// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_cluster_provider.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/omnibox/browser/actions/history_clusters_action.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "third_party/omnibox_proto/groups.pb.h"

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

void HistoryClusterProvider::Start(const AutocompleteInput& input,
                                   bool minimal_changes) {
  Stop(true, false);

  if (input.omit_asynchronous_matches())
    return;

  if (!IsJourneysEnabledInOmnibox(client_->GetHistoryClustersService(),
                                  client_->GetPrefs())) {
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
    if (client_->GetHistoryClustersService()->DoesQueryMatchAnyCluster(
            base::UTF16ToUTF8(search_match.contents))) {
      client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
          OmniboxTriggeredFeatureService::Feature::kHistoryClusterSuggestion);
      if (!history_clusters::GetConfig()
               .omnibox_history_cluster_provider_counterfactual) {
        matches_.push_back(CreateMatch(search_match.contents));
      }
      return true;
    }
  }
  return false;
}

AutocompleteMatch HistoryClusterProvider::CreateMatch(std::u16string text) {
  AutocompleteMatch match;
  match.provider = this;
  match.type = AutocompleteMatch::Type::HISTORY_CLUSTER;

  // TODO(manukh): Currently, history cluster suggestions only display when the
  //  `text` is an exact match of a cluster keyword, and all cluster keywords
  //  are treated equal. Therefore, we're limited to using a static value.
  //  Ideally, relevance would depend on how many keywords matched, how
  //  significant the keywords were, how significant their clusters were etc.
  match.relevance =
      history_clusters::GetConfig().omnibox_history_cluster_provider_score;

  match.fill_into_edit = base::UTF8ToUTF16(base::StringPrintf(
      "chrome://history/journeys?q=%s", base::UTF16ToUTF8(text).c_str()));

  match.destination_url = GURL(base::UTF8ToUTF16(base::StringPrintf(
      "chrome://history/journeys?q=%s",
      base::EscapeQueryParamValue(base::UTF16ToUTF8(text), /*use_plus=*/false)
          .c_str())));

  match.description = text;
  match.description_class = ClassifyTermMatches(
      FindTermMatches(input_.text(), text), text.length(),
      ACMatchClassification::MATCH, ACMatchClassification::NONE);

  match.contents = match.fill_into_edit;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));

  if (!history_clusters::GetConfig()
           .omnibox_history_cluster_provider_free_ranking) {
    match.suggestion_group_id = omnibox::GROUP_HISTORY_CLUSTER;
    // Insert a corresponding omnibox::GroupConfig with default values in the
    // suggestion groups map; otherwise the group ID will get dropped.
    suggestion_groups_map_[omnibox::GROUP_HISTORY_CLUSTER];
  }

  return match;
}
