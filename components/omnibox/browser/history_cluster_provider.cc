// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_cluster_provider.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/search_provider.h"

HistoryClusterProvider::HistoryClusterProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener,
    SearchProvider* search_provider)
    : AutocompleteProvider(AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER),
      client_(client),
      listener_(listener),
      search_provider_(search_provider) {
  DCHECK(search_provider_);
  search_provider_->AddListener(this);
}

void HistoryClusterProvider::Start(const AutocompleteInput& input,
                                   bool minimal_changes) {
  done_ = true;
  matches_.clear();

  if (!input.want_asynchronous_matches())
    return;

  if (!IsJourneysEnabledInOmnibox(client_->GetHistoryClustersService(),
                                  client_->GetPrefs())) {
    return;
  }

  if (!history_clusters::GetConfig().omnibox_history_cluster_provider)
    return;

  done_ = false;
  input_ = input;

  if (search_provider_->done())
    CreateMatches();
}

void HistoryClusterProvider::Stop(bool clear_cached_results,
                                  bool due_to_user_inactivity) {
  done_ = true;
}

void HistoryClusterProvider::OnProviderUpdate(bool updated_matches) {
  if (done_ || !search_provider_->done())
    return;
  listener_->OnProviderUpdate(CreateMatches());
}

bool HistoryClusterProvider::CreateMatches() {
  done_ = true;

  // Iterate search matches in their current order. This is usually highest to
  // lowest relevance with an exception for search-what-you-typed search
  // suggestions being ordered before others.
  for (const auto& search_match : search_provider_->matches()) {
    if (client_->GetHistoryClustersService()->DoesQueryMatchAnyCluster(
            base::UTF16ToUTF8(search_match.contents))) {
      matches_.push_back(CreateMatch(search_match.contents));
      return true;
    }
  }
  return false;
}

AutocompleteMatch HistoryClusterProvider::CreateMatch(std::u16string text) {
  AutocompleteMatch match;
  match.provider = this;
  match.type = AutocompleteMatch::Type::HISTORY_CLUSTER;

  // 900 seems to work well in local tests. It's high enough to
  // outscore search suggestions and therefore not be crowded out, but low
  // enough to only display when there aren't too many strong navigation
  // matches.
  // TODO(manukh): Currently, history cluster suggestions only display when the
  //  `text` is an exact match of a cluster keyword, and all cluster keywords
  //  are treated equal. Therefore, we're limited to using a static value.
  //  Ideally, relevance would depend on how many keywords matched, how
  //  significant the keywords were, how significant their clusters were etc.
  match.relevance = 900;

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

  // TODO(manukh): Ideally, this would read e.g. "Journey from Yesterday", but
  //  we don't currently track which keyword is connected to which history
  //  cluster.
  match.contents = u"Journey";
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::DIM));

  return match;
}
