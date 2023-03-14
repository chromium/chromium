// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class SearchCompanionPageHandler;

///////////////////////////////////////////////////////////////////////////////
// SearchCompanionSidePanelUI
//
//  A UI class to create the necessary handlers and bind the side panel view to
//  the mojo-driven UI that is contained within it.
//
class SearchCompanionSidePanelUI
    : public ui::MojoBubbleWebUIController,
      public side_panel::mojom::SearchCompanionPageHandlerFactory {
 public:
  explicit SearchCompanionSidePanelUI(content::WebUI* web_ui);
  SearchCompanionSidePanelUI(const SearchCompanionSidePanelUI&) = delete;
  SearchCompanionSidePanelUI& operator=(const SearchCompanionSidePanelUI&) =
      delete;
  ~SearchCompanionSidePanelUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          side_panel::mojom::SearchCompanionPageHandlerFactory> receiver);

  content::WebUI* GetWebUi();

 private:
  // side_panel::mojom::SearchCompanionPageHandlerFactory:
  void CreateSearchCompanionPageHandler(
      mojo::PendingReceiver<side_panel::mojom::SearchCompanionPageHandler>
          receiver,
      mojo::PendingRemote<side_panel::mojom::SearchCompanionPage> page)
      override;

  std::unique_ptr<SearchCompanionPageHandler> search_companion_page_handler_;
  mojo::Receiver<side_panel::mojom::SearchCompanionPageHandlerFactory>
      search_companion_page_factory_receiver_{this};
  raw_ptr<content::WebUI> web_ui_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_SIDE_PANEL_UI_H_
