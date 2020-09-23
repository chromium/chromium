// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui_embedder.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class TabSearchUI : public ui::MojoWebUIController,
                    public tab_search::mojom::PageHandlerFactory,
                    public TabSearchPageHandler::Delegate {
 public:
  explicit TabSearchUI(content::WebUI* web_ui);
  TabSearchUI(const TabSearchUI&) = delete;
  TabSearchUI& operator=(const TabSearchUI&) = delete;
  ~TabSearchUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver);
  void SetEmbedder(TabSearchUIEmbedder* embedder);

  // TabSearchPageHandler::Delegate:
  void ShowUI() override;
  void CloseUI() override;

 private:
  // tab_search::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<tab_search::mojom::Page> page,
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) override;

  std::unique_ptr<TabSearchPageHandler> page_handler_;

  mojo::Receiver<tab_search::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WebuiLoadTimer webui_load_timer_;

  TabSearchUIEmbedder* embedder_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_H_
