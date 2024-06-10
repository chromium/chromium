// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
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
      base::WeakPtr<Profile> profile);
  HistoryEmbeddingsHandler(const HistoryEmbeddingsHandler&) = delete;
  HistoryEmbeddingsHandler& operator=(const HistoryEmbeddingsHandler&) = delete;
  ~HistoryEmbeddingsHandler() override;

  // history_embeddings::mojom::PageHandler:
  void Search(history_embeddings::mojom::SearchQueryPtr query,
              SearchCallback callback) override;
  void RecordSearchResultsMetrics(bool non_empty_results,
                                  bool user_clicked_results) override;

  // Callback for querying `HistoryEmbeddingsService::Search()`.
  void OnReceivedSearchResult(SearchCallback callback,
                              history_embeddings::SearchResult result);

 private:
  mojo::Receiver<history_embeddings::mojom::PageHandler> page_handler_;

  // The profile is used to get the HistoryEmbeddingsService to fulfill
  // search requests.
  const base::WeakPtr<Profile> profile_;

  base::WeakPtrFactory<HistoryEmbeddingsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_
