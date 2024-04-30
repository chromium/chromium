// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/time_format.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HistoryEmbeddingsUserActions {
  kNonEmptyQueryHistorySearch = 0,
  kEmbeddingsSearch = 1,
  kEmbeddingsNonEmptyResultsShown = 2,
  kEmbeddingsResultClicked = 3,
  kMaxValue = kEmbeddingsResultClicked,
};

// Receives the results of a HistoryEmbeddingsService::Search call, builds
// them into mojom objects for the page, and sends them to the callback.
void OnSearchCompleted(HistoryEmbeddingsHandler::SearchCallback callback,
                       history_embeddings::SearchResult native_search_result) {
  auto mojom_search_result = history_embeddings::mojom::SearchResult::New();
  for (history_embeddings::ScoredUrlRow& scored_url_row :
       native_search_result) {
    auto item = history_embeddings::mojom::SearchResultItem::New();
    item->title = base::UTF16ToUTF8(scored_url_row.row.title());
    item->url = scored_url_row.row.url();
    item->source_passage = scored_url_row.scored_url.passage;
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

    mojom_search_result->items.push_back(std::move(item));
  }
  std::move(callback).Run(std::move(mojom_search_result));
}

}  // namespace

HistoryEmbeddingsHandler::HistoryEmbeddingsHandler(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
        pending_page_handler,
    base::WeakPtr<Profile> profile)
    : page_handler_(this, std::move(pending_page_handler)),
      profile_(std::move(profile)) {}

HistoryEmbeddingsHandler::~HistoryEmbeddingsHandler() = default;

void HistoryEmbeddingsHandler::Search(
    history_embeddings::mojom::SearchQueryPtr query,
    SearchCallback callback) {
  if (!profile_) {
    std::move(callback).Run(history_embeddings::mojom::SearchResult::New());
    return;
  }

  history_embeddings::HistoryEmbeddingsService* service =
      HistoryEmbeddingsServiceFactory::GetForProfile(profile_.get());
  // The service is never null. Even tests build and use a service.
  CHECK(service);
  service->Search(query->query, query->time_range_start,
                  history_embeddings::kSearchResultItemCount.Get(),
                  base::BindOnce(&OnSearchCompleted, std::move(callback)));
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
