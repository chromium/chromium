// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_embeddings_provider.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"

namespace {
constexpr int kMaxScore = 1000;

// TODO(crbug.com/364329824) Use real/mock answer from service.
AutocompleteMatch CreateMockAnswer(HistoryEmbeddingsProvider* provider) {
  AutocompleteMatch match(provider, 1 * kMaxScore, /*deletable=*/false,
                          AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER);
  match.destination_url = GURL("chrome://history");

  match.description = AutocompleteMatch::SanitizeString(
      u"The Matenadaran (Armenian: Մատենադարան), officially the Mesrop "
      u"Mashtots Institute of Ancient Manuscripts,[a] is a museum, "
      u"repository of manuscripts, and a research institute in Yerevan, "
      u"Armenia. It is the world's largest repository of Armenian "
      u"manuscripts.[5]\n"
      "\n"
      "It was established in 1959 on the basis of the nationalized "
      "collection of the Armenian Church, formerly held at Etchmiadzin. Its "
      "collection has gradually expanded since its establishment, mostly "
      "from individual donations. One of the most prominent landmarks of "
      "Yerevan, it is named after Mesrop Mashtots, the inventor of the "
      "Armenian alphabet, whose statue stands in front of the building. Its "
      "collection is included in the register of the UNESCO Memory of the "
      "World program.");
  match.description_class = {{0, ACMatchClassification::NONE}};

  match.contents = u"wikipedia.org/wiki/  •  Visited September 30,2024";
  match.contents_class = {{0, ACMatchClassification::DIM}};

  return match;
}

}  // namespace

HistoryEmbeddingsProvider::HistoryEmbeddingsProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS, client) {
  AddListener(listener);
}

HistoryEmbeddingsProvider::~HistoryEmbeddingsProvider() = default;

void HistoryEmbeddingsProvider::Start(const AutocompleteInput& input,
                                      bool minimal_changes) {
  done_ = true;
  matches_.clear();

  if (!client()->IsHistoryEmbeddingsEnabled()) {
    return;
  }

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto [adjusted_input, starter_pack_engine] =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client()->GetTemplateURLService());
  starter_pack_engine_ = starter_pack_engine;

  int num_terms =
      history_embeddings::CountWords(base::UTF16ToUTF8(adjusted_input.text()));
  if (num_terms < history_embeddings::kSearchQueryMinimumWordCount.Get())
    return;

  history_embeddings::HistoryEmbeddingsService* service =
      client()->GetHistoryEmbeddingsService();
  CHECK(service);
  last_search_input_ = adjusted_input.text();
  done_ = false;
  client()->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
      metrics::OmniboxEventProto_Feature_HISTORY_EMBEDDINGS_FEATURE);
  service->Search(
      nullptr, base::UTF16ToUTF8(adjusted_input.text()), {},
      provider_max_matches_,
      base::BindRepeating(&HistoryEmbeddingsProvider::OnReceivedSearchResult,
                          weak_factory_.GetWeakPtr(), adjusted_input.text()));

  if (history_embeddings::kAnswersInOmniboxScoped.Get()) {
    matches_.push_back(CreateMockAnswer(this));
    NotifyListeners(true);
  }
}

void HistoryEmbeddingsProvider::Stop(bool clear_cached_results,
                                     bool due_to_user_inactivity) {
  done_ = true;
  // TODO(b/333770460): Once `HistoryEmbeddingsService` has a stop API, we
  //   should call it here.
}

void HistoryEmbeddingsProvider::OnReceivedSearchResult(
    std::u16string input_text,
    history_embeddings::SearchResult result) {
  // Check `done_` in case the stop timer fired or the user closed the omnibox
  // before `Search()` completed. Check `last_search_input_` in case this is the
  // result for an earlier `Search()` request; there's usually 2 requests
  // ongoing as the user types.
  if (done_ || last_search_input_ != input_text)
    return;

  for (const history_embeddings::ScoredUrlRow& scored_url_row :
       result.scored_url_rows) {
    AutocompleteMatch match(this, scored_url_row.scored_url.score * kMaxScore,
                            client()->AllowDeletingBrowserHistory(),
                            AutocompleteMatchType::HISTORY_EMBEDDINGS);
    match.destination_url = scored_url_row.row.url();

    match.description =
        AutocompleteMatch::SanitizeString(scored_url_row.row.title());
    match.description_class = ClassifyTermMatches(
        FindTermMatches(input_text, match.description),
        match.description.size(), ACMatchClassification::MATCH,
        ACMatchClassification::NONE);

    match.contents = base::UTF8ToUTF16(scored_url_row.row.url().spec());
    match.contents_class = ClassifyTermMatches(
        FindTermMatches(input_text, match.contents), match.contents.size(),
        ACMatchClassification::MATCH, ACMatchClassification::URL);

    if (starter_pack_engine_) {
      match.keyword = starter_pack_engine_->keyword();
      match.transition = ui::PAGE_TRANSITION_KEYWORD;
    }

    match.RecordAdditionalInfo("passages", scored_url_row.GetBestPassage());

    matches_.push_back(match);
  }

  done_ = true;
  NotifyListeners(!matches_.empty());
}
