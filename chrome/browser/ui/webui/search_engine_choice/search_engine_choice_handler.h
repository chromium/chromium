// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class SearchEngineChoiceHandler
    : public search_engine_choice::mojom::PageHandler {
 public:
  explicit SearchEngineChoiceHandler(
      mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver,
      base::OnceCallback<void(int)> display_dialog_callback,
      base::OnceCallback<void(int)> handle_choice_selected_callback);

  SearchEngineChoiceHandler(const SearchEngineChoiceHandler&) = delete;
  SearchEngineChoiceHandler& operator=(const SearchEngineChoiceHandler&) =
      delete;

  ~SearchEngineChoiceHandler() override;

  // search_engine_choice::mojom::PageHandler:
  void DisplayDialog(uint32_t content_height) override;
  void HandleSearchEngineChoiceSelected(int32_t prepopulate_id) override;

 private:
  mojo::Receiver<search_engine_choice::mojom::PageHandler> receiver_;
  base::OnceCallback<void(int)> display_dialog_callback_;
  base::OnceCallback<void(int)> handle_choice_selected_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_HANDLER_H_
