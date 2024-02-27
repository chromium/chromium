// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

HistoryEmbeddingsHandler::HistoryEmbeddingsHandler(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
        pending_page_handler)
    : page_handler_(this, std::move(pending_page_handler)) {}

HistoryEmbeddingsHandler::~HistoryEmbeddingsHandler() = default;

void HistoryEmbeddingsHandler::DoSomething(DoSomethingCallback callback) {
  std::move(callback).Run(true);
}
