// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice.mojom.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

// The WebUI controller for `chrome://search-engine-choice`.
class SearchEngineChoiceUI
    : public ui::MojoWebUIController,
      public search_engine_choice::mojom::PageHandlerFactory {
 public:
  explicit SearchEngineChoiceUI(content::WebUI* web_ui);

  SearchEngineChoiceUI(const SearchEngineChoiceUI&) = delete;
  SearchEngineChoiceUI& operator=(const SearchEngineChoiceUI&) = delete;

  ~SearchEngineChoiceUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<search_engine_choice::mojom::PageHandlerFactory>
          receiver);

  // Initializes the callbacks that need to be passed to the handler.
  // `display_dialog_callback` is how we display the search engine choice
  // dialog. It will be called when the page's static content is rendered.
  void Initialize(base::OnceCallback<void(int)> display_dialog_callback);

 private:
  // search_engine_choice::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver)
      override;

  // Notifies the search engine choice service that a choice has been made.
  void HandleSearchEngineChoiceMade(int prepopulate_id);

  std::unique_ptr<SearchEngineChoiceHandler> page_handler_;

  mojo::Receiver<search_engine_choice::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  base::OnceCallback<void(int)> display_dialog_callback_;
  const raw_ref<Profile> profile_;
  base::WeakPtrFactory<SearchEngineChoiceUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_H_
