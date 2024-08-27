// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "history_embeddings_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

namespace {

optimization_guide::proto::UserFeedback
OptimizationFeedbackFromMojoUserFeedback(
    history_embeddings::mojom::UserFeedback feedback) {
  switch (feedback) {
    case history_embeddings::mojom::UserFeedback::kUserFeedbackPositive:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP;
    case history_embeddings::mojom::UserFeedback::kUserFeedbackNegative:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN;
    case history_embeddings::mojom::UserFeedback::kUserFeedbackUnspecified:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
  }
}

}  // namespace

HistoryEmbeddingsHandler::HistoryEmbeddingsHandler(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
        pending_page_handler,
    base::WeakPtr<Profile> profile,
    content::WebUI* web_ui)
    : page_handler_(this, std::move(pending_page_handler)),
      profile_(std::move(profile)),
      web_ui_(web_ui) {}

HistoryEmbeddingsHandler::~HistoryEmbeddingsHandler() = default;

void HistoryEmbeddingsHandler::SetPage(
    mojo::PendingRemote<history_embeddings::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void HistoryEmbeddingsHandler::Search(
    history_embeddings::mojom::SearchQueryPtr query) {
  if (!profile_) {
    OnReceivedSearchResult({});
    return;
  }

  history_embeddings::HistoryEmbeddingsService* service =
      HistoryEmbeddingsServiceFactory::GetForProfile(profile_.get());
  // The service is never null. Even tests build and use a service.
  CHECK(service);
  service->Search(
      query->query, query->time_range_start,
      history_embeddings::kSearchResultItemCount.Get(),
      base::BindRepeating(&HistoryEmbeddingsHandler::OnReceivedSearchResult,
                          weak_ptr_factory_.GetWeakPtr()));
}

void HistoryEmbeddingsHandler::OnReceivedSearchResult(
    history_embeddings::SearchResult native_search_result) {
  last_result_ = std::move(native_search_result);
  user_feedback_ =
      optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;

  auto mojom_search_result = history_embeddings::mojom::SearchResult::New();
  mojom_search_result->query = last_result_.query;
  bool has_answer = history_embeddings::kEnableAnswers.Get() &&
                    !last_result_.AnswerText().empty();
  if (has_answer) {
    mojom_search_result->answer = last_result_.AnswerText();
  }
  for (size_t i = 0; i < last_result_.scored_url_rows.size(); i++) {
    history_embeddings::ScoredUrlRow& scored_url_row =
        last_result_.scored_url_rows[i];
    auto item = history_embeddings::mojom::SearchResultItem::New();
    item->title = base::UTF16ToUTF8(scored_url_row.row.title());
    item->url = scored_url_row.row.url();
    item->relative_time = base::UTF16ToUTF8(ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
        base::Time::Now() - scored_url_row.row.last_visit()));
    item->last_url_visit_timestamp =
        scored_url_row.row.last_visit().InMillisecondsFSinceUnixEpoch();

    url_formatter::FormatUrlTypes format_types =
        url_formatter::kFormatUrlOmitDefaults |
        url_formatter::kFormatUrlOmitHTTPS |
        url_formatter::kFormatUrlOmitTrivialSubdomains;
    item->url_for_display = base::UTF16ToUTF8(url_formatter::FormatUrl(
        scored_url_row.row.url(), format_types, base::UnescapeRule::SPACES,
        nullptr, nullptr, nullptr));
    if (has_answer && i == last_result_.AnswerIndex()) {
      item->answer_data = history_embeddings::mojom::AnswerData::New();
      item->answer_data->answer_text_directives.assign(
          last_result_.answerer_result.text_directives.begin(),
          last_result_.answerer_result.text_directives.end());
    }

    if (history_embeddings::kShowSourcePassages.Get()) {
      item->source_passage = scored_url_row.GetBestPassage();
    }

    mojom_search_result->items.push_back(std::move(item));
  }
  page_->SearchResultChanged(std::move(mojom_search_result));
}

void HistoryEmbeddingsHandler::SendQualityLog(
    const std::vector<uint32_t>& selected_indices,
    uint32_t num_chars_for_query) {
  history_embeddings::HistoryEmbeddingsService* service =
      HistoryEmbeddingsServiceFactory::GetForProfile(profile_.get());
  std::set<size_t> indices_set(selected_indices.begin(),
                               selected_indices.end());
  service->SendQualityLog(last_result_, user_feedback_, indices_set,
                          num_chars_for_query, false);
}

void HistoryEmbeddingsHandler::RecordSearchResultsMetrics(
    bool non_empty_results,
    bool user_clicked_results) {
  base::UmaHistogramEnumeration(
      "History.Embeddings.UserActions",
      HistoryEmbeddingsUserActions::kEmbeddingsSearch);
  if (non_empty_results) {
    base::UmaHistogramEnumeration(
        "History.Embeddings.UserActions",
        HistoryEmbeddingsUserActions::kEmbeddingsNonEmptyResultsShown);
  }
  if (user_clicked_results) {
    base::UmaHistogramEnumeration(
        "History.Embeddings.UserActions",
        HistoryEmbeddingsUserActions::kEmbeddingsResultClicked);
  }
}

void HistoryEmbeddingsHandler::SetUserFeedback(
    history_embeddings::mojom::UserFeedback user_feedback) {
  user_feedback_ = OptimizationFeedbackFromMojoUserFeedback(user_feedback);
  if (user_feedback ==
      history_embeddings::mojom::UserFeedback::kUserFeedbackNegative) {
    Browser* browser = chrome::FindLastActive();
    if (!browser) {
      return;
    }

    chrome::ShowFeedbackPage(
        browser, feedback::kFeedbackSourceAI,
        /*description_template=*/std::string(),
        /*description_placeholder_text=*/
        l10n_util::GetStringUTF8(IDS_HISTORY_EMBEDDINGS_FEEDBACK_PLACEHOLDER),
        /*category_tag=*/"genai_history",
        /*extra_diagnostics=*/std::string(),
        /*autofill_metadata=*/base::Value::Dict(), base::Value::Dict());
  }
}

void HistoryEmbeddingsHandler::MaybeShowFeaturePromo() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui_->GetWebContents());
  if (!browser) {
    return;
  }
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHHistorySearchFeature);
}
