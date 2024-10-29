// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/history_clusters/core/history_clusters_util.h"
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

history_embeddings::mojom::AnswerStatus AnswererAnswerStatusToMojoAnswerStatus(
    history_embeddings::ComputeAnswerStatus status) {
  switch (status) {
    case history_embeddings::ComputeAnswerStatus::kUnspecified:
      return history_embeddings::mojom::AnswerStatus::kUnspecified;
    case history_embeddings::ComputeAnswerStatus::kSuccess:
      return history_embeddings::mojom::AnswerStatus::kSuccess;
    case history_embeddings::ComputeAnswerStatus::kUnanswerable:
      return history_embeddings::mojom::AnswerStatus::kUnanswerable;
    case history_embeddings::ComputeAnswerStatus::kModelUnavailable:
      return history_embeddings::mojom::AnswerStatus::kModelUnavailable;
    case history_embeddings::ComputeAnswerStatus::kExecutionFailure:
      return history_embeddings::mojom::AnswerStatus::kExecutionFailure;
    case history_embeddings::ComputeAnswerStatus::kExecutionCancelled:
      return history_embeddings::mojom::AnswerStatus::kExecutionCanceled;
    case history_embeddings::ComputeAnswerStatus::kLoading:
      return history_embeddings::mojom::AnswerStatus::kLoading;
    case history_embeddings::ComputeAnswerStatus::kFiltered:
      return history_embeddings::mojom::AnswerStatus::kFiltered;
  }
}

}  // namespace

HistoryEmbeddingsHandler::HistoryEmbeddingsHandler(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
        pending_page_handler,
    base::WeakPtr<Profile> profile,
    content::WebUI* web_ui,
    bool for_side_panel)
    : page_handler_(this, std::move(pending_page_handler)),
      for_side_panel_(for_side_panel),
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
  last_result_ = service->Search(
      &last_result_, query->query, query->time_range_start,
      history_embeddings::kSearchResultItemCount.Get(),
      base::BindRepeating(&HistoryEmbeddingsHandler::OnReceivedSearchResult,
                          weak_ptr_factory_.GetWeakPtr()));
  VLOG(3) << "HistoryEmbeddingsHandler::Search started for '"
          << last_result_.query << "'";
}

void HistoryEmbeddingsHandler::PublishResultToPageForTesting(
    const history_embeddings::SearchResult& native_search_result) {
  PublishResultToPage(native_search_result);
}

void HistoryEmbeddingsHandler::PublishResultToPage(
    const history_embeddings::SearchResult& native_search_result) {
  user_feedback_ =
      optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;

  auto mojom_search_result = history_embeddings::mojom::SearchResult::New();
  mojom_search_result->query = native_search_result.query;

  bool has_answer = false;
  if (history_embeddings::IsHistoryEmbeddingsAnswersEnabled()) {
    mojom_search_result->answer_status = AnswererAnswerStatusToMojoAnswerStatus(
        native_search_result.answerer_result.status);
    if (!native_search_result.AnswerText().empty()) {
      has_answer = true;
      mojom_search_result->answer = native_search_result.AnswerText();
    }
  }

  for (size_t i = 0; i < native_search_result.scored_url_rows.size(); i++) {
    const history_embeddings::ScoredUrlRow& scored_url_row =
        native_search_result.scored_url_rows[i];
    auto item = history_embeddings::mojom::SearchResultItem::New();
    item->title = base::UTF16ToUTF8(scored_url_row.row.title());
    item->url = scored_url_row.row.url();
    item->relative_time = base::UTF16ToUTF8(ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
        base::Time::Now() - scored_url_row.row.last_visit()));
    item->short_date_time = base::UTF16ToUTF8(
        base::TimeFormatShortDate(scored_url_row.row.last_visit()));
    item->last_url_visit_timestamp =
        scored_url_row.row.last_visit().InMillisecondsFSinceUnixEpoch();
    item->url_for_display =
        base::UTF16ToUTF8(history_clusters::ComputeURLForDisplay(
            scored_url_row.row.url(),
            history_embeddings::kTrimAfterHostInResults.Get()));
    if (has_answer && i == native_search_result.AnswerIndex()) {
      item->answer_data = history_embeddings::mojom::AnswerData::New();
      item->answer_data->answer_text_directives.assign(
          native_search_result.answerer_result.text_directives.begin(),
          native_search_result.answerer_result.text_directives.end());
    }

    if (history_embeddings::kShowSourcePassages.Get()) {
      item->source_passage = scored_url_row.GetBestPassage();
    }

    mojom_search_result->items.push_back(std::move(item));
  }
  page_->SearchResultChanged(std::move(mojom_search_result));
}

void HistoryEmbeddingsHandler::OnReceivedSearchResult(
    history_embeddings::SearchResult native_search_result) {
  // Ignore results for outdated queries; only update results for current query.
  if (!native_search_result.IsContinuationOf(last_result_)) {
    VLOG(3) << "HistoryEmbeddingsHandler::OnReceivedSearchResult for '"
            << native_search_result.query
            << "' (dropped as not a continuation of '" << last_result_.query
            << "')";
    return;
  }
  VLOG(3) << "HistoryEmbeddingsHandler::OnReceivedSearchResult for '"
          << native_search_result.query << "' (published as continuation)";
  last_result_ = std::move(native_search_result);
  PublishResultToPage(last_result_);
}

void HistoryEmbeddingsHandler::SendQualityLog(
    const std::vector<uint32_t>& selected_indices,
    uint32_t num_chars_for_query) {
  history_embeddings::HistoryEmbeddingsService* service =
      HistoryEmbeddingsServiceFactory::GetForProfile(profile_.get());
  std::set<size_t> indices_set(selected_indices.begin(),
                               selected_indices.end());
  service->SendQualityLog(
      last_result_, indices_set, num_chars_for_query, user_feedback_,
      for_side_panel_
          ? optimization_guide::proto::UiSurface::UI_SURFACE_SIDE_PANEL
          : optimization_guide::proto::UiSurface::UI_SURFACE_HISTORY_PAGE);
}

void HistoryEmbeddingsHandler::RecordSearchResultsMetrics(
    bool non_empty_results,
    bool user_clicked_results,
    bool answer_shown,
    bool answer_citation_clicked,
    bool other_history_result_clicked,
    uint32_t query_word_count) {
  auto record_histograms = [&](const std::string& histogram_name) {
    base::UmaHistogramEnumeration(
        histogram_name, HistoryEmbeddingsUserActions::kEmbeddingsSearch);
    if (non_empty_results) {
      base::UmaHistogramEnumeration(
          histogram_name,
          HistoryEmbeddingsUserActions::kEmbeddingsNonEmptyResultsShown);
    }
    if (user_clicked_results) {
      base::UmaHistogramEnumeration(
          histogram_name,
          HistoryEmbeddingsUserActions::kEmbeddingsResultClicked);
    }
    if (answer_shown) {
      base::UmaHistogramEnumeration(histogram_name,
                                    HistoryEmbeddingsUserActions::kAnswerShown);
    }
    if (answer_citation_clicked) {
      base::UmaHistogramEnumeration(
          histogram_name, HistoryEmbeddingsUserActions::kAnswerCitationClicked);
    }
    if (other_history_result_clicked) {
      base::UmaHistogramEnumeration(
          histogram_name,
          HistoryEmbeddingsUserActions::kOtherHistoryResultClicked);
    }
  };

  // Unsliced original user actions histogram.
  record_histograms("History.Embeddings.UserActions");

  // Sliced versions for side panel and history page.
  if (for_side_panel_) {
    record_histograms("History.Embeddings.UserActions.SidePanel");
  } else {
    record_histograms("History.Embeddings.UserActions.HistoryPage");
  }

  if (answer_shown &&
      base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForHistoryEmbeddings)) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        profile_.get(), /*create_if_necessary=*/true);
    CHECK(hats_service);

    base::TimeDelta delay_time =
        features::kHappinessTrackingSurveysForHistoryEmbeddingsDelayTime.Get();
    hats_service->LaunchDelayedSurvey(
        kHatsSurveyTriggerHistoryEmbeddings, delay_time.InMilliseconds(),
        {{"non empty results", non_empty_results},
         {"best matches result clicked", user_clicked_results},
         {"result clicked", other_history_result_clicked},
         {"answer shown", answer_shown},
         {"answer citation clicked", answer_citation_clicked}},
        {{"query word count", base::NumberToString(query_word_count)}});
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

void HistoryEmbeddingsHandler::OpenSettingsPage() {
  NavigateParams navigate_params(profile_.get(),
                                 GURL(chrome::kHistorySearchSettingURL),
                                 ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::SHOW_WINDOW;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
}

void HistoryEmbeddingsHandler::MaybeShowFeaturePromo() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui_->GetWebContents());
  if (!browser) {
    return;
  }
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHHistorySearchFeature);
}
