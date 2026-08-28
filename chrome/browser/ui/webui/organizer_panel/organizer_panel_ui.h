// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_

#include <memory>
#include <string_view>

#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class TabSearchPageHandler;
class OrganizerPanelUI;

class OrganizerPanelUIConfig
    : public DefaultTopChromeWebUIConfig<OrganizerPanelUI> {
 public:
  OrganizerPanelUIConfig();
};

class OrganizerPanelUI : public TopChromeWebUIController,
                         public tab_search::mojom::PageHandlerFactory {
 public:
  explicit OrganizerPanelUI(content::WebUI* web_ui);
  ~OrganizerPanelUI() override;

  OrganizerPanelUI(const OrganizerPanelUI&) = delete;
  OrganizerPanelUI& operator=(const OrganizerPanelUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver);

  static constexpr std::string_view GetWebUIName() { return "OrganizerPanel"; }

 private:
  // tab_search::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<tab_search::mojom::Page> page,
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) override;

  std::unique_ptr<TabSearchPageHandler> page_handler_;
  mojo::Receiver<tab_search::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
