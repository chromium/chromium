// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HistoryEmbeddingsUserActions {
  kNonEmptyQueryHistorySearch = 0,
  kEmbeddingsSearch = 1,
  kEmbeddingsNonEmptyResultsShown = 2,
  kEmbeddingsResultClicked = 3,
  kMaxValue = kEmbeddingsResultClicked,
};

class HistoryEmbeddingsHandler : public history_embeddings::mojom::PageHandler {
 public:
  HistoryEmbeddingsHandler(
      mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
          pending_page_handler,
      base::WeakPtr<Profile> profile,
      content::WebUI* web_ui);
  HistoryEmbeddingsHandler(const HistoryEmbeddingsHandler&) = delete;
  HistoryEmbeddingsHandler& operator=(const HistoryEmbeddingsHandler&) = delete;
  ~HistoryEmbeddingsHandler() override;

  // history_embeddings::mojom::PageHandler:
  void SetPage(mojo::PendingRemote<history_embeddings::mojom::Page>
                   pending_page) override;
  void Search(history_embeddings::mojom::SearchQueryPtr query) override;
  void RecordSearchResultsMetrics(bool non_empty_results,
                                  bool user_clicked_results) override;
  void SetUserFeedback(
      history_embeddings::mojom::UserFeedback user_feedback) override;
  void MaybeShowFeaturePromo() override;
  void SendQualityLog(const std::vector<uint32_t>& selected_indices,
                      uint32_t num_chars_for_query) override;
  void OpenSettingsPage() override;

  void PublishResultToPageForTesting(
      const history_embeddings::SearchResult& native_search_result);

 private:
  // Builds mojom result and publishes it to the browser page UI.
  void PublishResultToPage(
      const history_embeddings::SearchResult& native_search_result);

  // Callback for querying `HistoryEmbeddingsService::Search()`.
  void OnReceivedSearchResult(history_embeddings::SearchResult result);

  mojo::Receiver<history_embeddings::mojom::PageHandler> page_handler_;
  mojo::Remote<history_embeddings::mojom::Page> page_;

  // The profile is used to get the HistoryEmbeddingsService to fulfill
  // search requests.
  const base::WeakPtr<Profile> profile_;

  raw_ptr<content::WebUI> web_ui_;

  history_embeddings::SearchResult last_result_;
  optimization_guide::proto::UserFeedback user_feedback_;

  base::WeakPtrFactory<HistoryEmbeddingsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_
