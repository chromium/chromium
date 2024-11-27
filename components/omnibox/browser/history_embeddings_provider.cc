// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_embeddings_provider.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// The max relevance a history embedding match will receive.
constexpr int kMaxRelevance = 1000;

// Whether the received `status` is expected to be the last or there should be
// more updates..
bool IsAnswerDone(history_embeddings::ComputeAnswerStatus status) {
  switch (status) {
    case history_embeddings::ComputeAnswerStatus::kUnspecified:
    case history_embeddings::ComputeAnswerStatus::kLoading:
      return false;
    case history_embeddings::ComputeAnswerStatus::kSuccess:
    case history_embeddings::ComputeAnswerStatus::kUnanswerable:
    case history_embeddings::ComputeAnswerStatus::kModelUnavailable:
    case history_embeddings::ComputeAnswerStatus::kExecutionFailure:
    case history_embeddings::ComputeAnswerStatus::kExecutionCancelled:
    case history_embeddings::ComputeAnswerStatus::kFiltered:
      return true;
  }
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

  if (input.omit_asynchronous_matches())
    return;

  if (!client()->IsHistoryEmbeddingsEnabled()) {
    return;
  }

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto [adjusted_input, starter_pack_engine] =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client()->GetTemplateURLService());
  input_ = adjusted_input;
  starter_pack_engine_ = starter_pack_engine;

  int num_terms =
      history_embeddings::CountWords(base::UTF16ToUTF8(adjusted_input.text()));
  if (num_terms < history_embeddings::kSearchQueryMinimumWordCount.Get())
    return;

  history_embeddings::HistoryEmbeddingsService* service =
      client()->GetHistoryEmbeddingsService();
  CHECK(service);
  done_ = false;
  client()->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
      metrics::OmniboxEventProto_Feature_HISTORY_EMBEDDINGS_FEATURE);
  service->Search(
      nullptr, base::UTF16ToUTF8(adjusted_input.text()), {},
      provider_max_matches_,
      base::BindRepeating(&HistoryEmbeddingsProvider::OnReceivedSearchResult,
                          weak_factory_.GetWeakPtr()));
}

void HistoryEmbeddingsProvider::Stop(bool clear_cached_results,
                                     bool due_to_user_inactivity) {
  // TODO(crbug.com/364303536): Ignore the stop timer since we know answers take
  //   longer than 1500ms to generate. This inadvertently also ignores stops
  //   caused by user action. A real fix is for providers to inform the
  //   controller that they expect a slow response and the controller to
  //   accommodate it by updating its stop, debounce, and cache timers'
  //   behaviors.
  if (!due_to_user_inactivity && !done_) {
    done_ = true;
    size_t erased_count = std::erase_if(matches_, [&](const auto& match) {
      return match.type == AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER;
    });
    CHECK_LE(erased_count, 1u);
    if (erased_count)
      NotifyListeners(!matches_.empty());
  }

  // TODO(b/333770460): Once `HistoryEmbeddingsService` has a stop API, we
  //   should call it here.
}

void HistoryEmbeddingsProvider::OnReceivedSearchResult(
    history_embeddings::SearchResult search_result) {
  // Check `done_` in case the stop timer fired or the user closed the omnibox
  // before `Search()` completed. Check `last_search_input_` in case this is the
  // result for an earlier `Search()` request; there's usually 2 requests
  // ongoing as the user types.
  if (done_ || search_result.query != base::UTF16ToUTF8(input_.text()))
    return;

  // `OnReceivedSearchResult()` can be called multiple times per `Search()`
  // request. Clear `matches_``to avoid aggregating duplicates.
  matches_.clear();

  if (search_result.scored_url_rows.empty())
    return;

  for (const history_embeddings::ScoredUrlRow& scored_url_row :
       search_result.scored_url_rows) {
    matches_.push_back(CreateMatch(scored_url_row));
  }

  bool answers_enabled = history_embeddings::kAnswersInOmniboxScoped.Get() &&
                         input_.InKeywordMode();
  if (answers_enabled) {
    auto optional_match = CreateAnswerMatch(
        search_result.answerer_result,
        search_result.scored_url_rows[search_result.AnswerIndex()],
        matches_[search_result.AnswerIndex()]);
    if (optional_match)
      matches_.push_back(optional_match.value());
  }

  done_ =
      !answers_enabled || IsAnswerDone(search_result.answerer_result.status);
  NotifyListeners(!matches_.empty());
}

AutocompleteMatch HistoryEmbeddingsProvider::CreateMatch(
    const history_embeddings::ScoredUrlRow& scored_url_row) {
  AutocompleteMatch match(
      this, std::min(scored_url_row.scored_url.score, 1.f) * kMaxRelevance,
      client()->AllowDeletingBrowserHistory(),
      AutocompleteMatchType::HISTORY_EMBEDDINGS);
  match.destination_url = scored_url_row.row.url();

  match.description =
      AutocompleteMatch::SanitizeString(scored_url_row.row.title());
  match.description_class = ClassifyTermMatches(
      FindTermMatches(input_.text(), match.description),
      match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  match.contents = base::UTF8ToUTF16(scored_url_row.row.url().spec());
  match.contents_class = ClassifyTermMatches(
      FindTermMatches(input_.text(), match.contents), match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  if (starter_pack_engine_) {
    match.keyword = starter_pack_engine_->keyword();
    match.transition = ui::PAGE_TRANSITION_KEYWORD;
  }

  match.RecordAdditionalInfo("passages", scored_url_row.GetBestPassage());

  return match;
}

std::optional<AutocompleteMatch> HistoryEmbeddingsProvider::CreateAnswerMatch(
    const history_embeddings::AnswererResult& answerer_result,
    const history_embeddings::ScoredUrlRow& scored_url_row,
    const AutocompleteMatch& match) {
  // If the match is outscored and not shown, then the answer shouldn't show
  // either.
  int score = std::max(match.relevance - 1, 1);

  switch (answerer_result.status) {
    case history_embeddings::ComputeAnswerStatus::kUnspecified:
    case history_embeddings::ComputeAnswerStatus::kUnanswerable:
    case history_embeddings::ComputeAnswerStatus::kFiltered:
    case history_embeddings::ComputeAnswerStatus::kExecutionCancelled:
    case history_embeddings::ComputeAnswerStatus::kModelUnavailable:
      return std::nullopt;

    case history_embeddings::ComputeAnswerStatus::kLoading: {
      AutocompleteMatch answer_match = CreateAnswerMatchHelper(
          score,
          l10n_util::GetStringUTF16(
              IDS_HISTORY_EMBEDDINGS_ANSWER_LOADING_HEADING),
          u"");
      answer_match.history_embeddings_answer_header_loading = true;
      return answer_match;
    }

    case history_embeddings::ComputeAnswerStatus::kSuccess: {
      AutocompleteMatch answer_match = CreateAnswerMatchHelper(
          score,
          l10n_util::GetStringUTF16(IDS_HISTORY_EMBEDDINGS_ANSWER_HEADING),
          AutocompleteMatch::SanitizeString(
              base::UTF8ToUTF16(answerer_result.answer.text())));
      answer_match.destination_url =
          GURL{"chrome://history/?q=" + answerer_result.query};
      std::u16string source = history_clusters::ComputeURLForDisplay(
          scored_url_row.row.url(),
          history_embeddings::kTrimAfterHostInResults.Get());
      answer_match.contents = AutocompleteMatch::SanitizeString(
          source + u"  •  " +
          l10n_util::GetStringFUTF16(
              IDS_HISTORY_EMBEDDINGS_ANSWER_SOURCE_VISIT_DATE_LABEL,
              base::TimeFormatShortDate(scored_url_row.row.last_visit())));
      answer_match.contents_class = {{0, ACMatchClassification::DIM}};
      return answer_match;
    }

    case history_embeddings::ComputeAnswerStatus::kExecutionFailure:
      return CreateAnswerMatchHelper(
          score,
          l10n_util::GetStringUTF16(IDS_HISTORY_EMBEDDINGS_ANSWER_HEADING),
          l10n_util::GetStringUTF16(
              IDS_HISTORY_EMBEDDINGS_ANSWERER_ERROR_TRY_AGAIN));
  }
}

AutocompleteMatch HistoryEmbeddingsProvider::CreateAnswerMatchHelper(
    int score,
    const std::u16string& history_embeddings_answer_header_text,
    const std::u16string& description) {
  AutocompleteMatch match(this, score, /*deletable=*/false,
                          AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER);
  match.history_embeddings_answer_header_text =
      history_embeddings_answer_header_text;
  match.description = description;
  if (!description.empty())
    match.description_class = {{0, ACMatchClassification::NONE}};
  match.RecordAdditionalInfo("history_embeddings_answer_header_text",
                             history_embeddings_answer_header_text);
  if (starter_pack_engine_) {
    match.keyword = starter_pack_engine_->keyword();
    match.transition = ui::PAGE_TRANSITION_KEYWORD;
  }
  return match;
}
