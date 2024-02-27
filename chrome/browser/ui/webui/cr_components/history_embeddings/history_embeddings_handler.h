// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

class HistoryEmbeddingsHandler : public history_embeddings::mojom::PageHandler {
 public:
  explicit HistoryEmbeddingsHandler(
      mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
          pending_page_handler);
  HistoryEmbeddingsHandler(const HistoryEmbeddingsHandler&) = delete;
  HistoryEmbeddingsHandler& operator=(const HistoryEmbeddingsHandler&) = delete;
  ~HistoryEmbeddingsHandler() override;

  // history_embeddings::mojom::PageHandler:
  void DoSomething(DoSomethingCallback callback) override;

 private:
  mojo::Receiver<history_embeddings::mojom::PageHandler> page_handler_;
  base::WeakPtrFactory<HistoryEmbeddingsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_HANDLER_H_
