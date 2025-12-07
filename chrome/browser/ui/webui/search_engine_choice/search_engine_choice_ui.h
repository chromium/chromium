// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice.mojom.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;
class SearchEngineChoiceUI;

class SearchEngineChoiceUIConfig
    : public content::DefaultWebUIConfig<SearchEngineChoiceUI> {
 public:
  SearchEngineChoiceUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISearchEngineChoiceHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

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

  // Initializes the callbacks that may be passed to the handler.
  // `display_dialog_callback` is called when the page's static content is
  // rendered and can be used to trigger the display of the page in a
  // dialog.
  // `on_choice_made_callback` is called once the user made a choice in
  // the UI.
  // `entry_point` is the view in which the UI is rendered.
  // The callbacks may be empty.
  void Initialize(base::OnceClosure display_dialog_callback,
                  base::OnceClosure on_choice_made_callback,
                  SearchEngineChoiceDialogService::EntryPoint entry_point);

 private:
  // search_engine_choice::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver)
      override;

  // Notifies the search engine choice service that a choice has been made.
  void HandleSearchEngineChoiceMade(int prepopulate_id,
                                    bool save_guest_mode_selection);

  // Notifies the search engine choice service that the learn more link was
  // clicked.
  void HandleLearnMoreLinkClicked();

  // Notifies the search engine choice service that the more button was clicked.
  void HandleMoreButtonClicked();

  std::unique_ptr<SearchEngineChoiceHandler> page_handler_;

  mojo::Receiver<search_engine_choice::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  // Displays the native dialog. May be null if the dialog is not triggered by
  // this UI.
  base::OnceClosure display_dialog_callback_;

  // Called when the choice is complete.
  base::OnceClosure on_choice_made_callback_;

  // The view in which the UI is rendered.
  SearchEngineChoiceDialogService::EntryPoint entry_point_;
  const raw_ref<Profile> profile_;
  base::WeakPtrFactory<SearchEngineChoiceUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_H_
