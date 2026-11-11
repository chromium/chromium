// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs.mojom.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "chrome/browser/ui/webui/organizer_panel/tab_groups.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class ForeignTabsPageHandler;
class OrganizerPanelPageHandler;
class Profile;
class SearchHandler;
class TabGroupsOrganizerPageHandler;
class TabSearchPageHandler;
class OrganizerPanelUI;

class OrganizerPanelUIConfig
    : public DefaultTopChromeWebUIConfig<OrganizerPanelUI> {
 public:
  OrganizerPanelUIConfig();

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool IsPreloadable() override;
  std::optional<int> GetCommandIdForTesting() override;
  std::optional<GURL> GetPreloadEngagementAliasURL() const override;
};

class OrganizerPanelUI
    : public TopChromeWebUIController,
      public organizer_panel::mojom::ForeignTabsPageHandlerFactory,
      public organizer_panel::mojom::PageHandlerFactory,
      public tab_search::mojom::PageHandlerFactory,
      public organizer_panel::mojom::TabGroupsOrganizerPageHandlerFactory {
 public:
  explicit OrganizerPanelUI(content::WebUI* web_ui);
  ~OrganizerPanelUI() override;

  OrganizerPanelUI(const OrganizerPanelUI&) = delete;
  OrganizerPanelUI& operator=(const OrganizerPanelUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<
          organizer_panel::mojom::ForeignTabsPageHandlerFactory> receiver);
  void BindInterface(
      mojo::PendingReceiver<organizer_panel::mojom::PageHandlerFactory>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver);
  void BindInterface(
      mojo::PendingReceiver<tab_search::mojom::SearchHandler> receiver);
  void BindInterface(
      mojo::PendingReceiver<
          organizer_panel::mojom::TabGroupsOrganizerPageHandlerFactory>
          receiver);

  static constexpr std::string_view GetWebUIName() { return "OrganizerPanel"; }

  // Returns the loadTimeData values that depend on `profile`, the profile of
  // the window that shows the panel. They are in the WebUIDataSource, or passed
  // to each frame in WebUIRenderFrameCreated() if the data source is shared by
  // the windows of all profiles, i.e. in TopChromeWebUIProfile.
  // TODO(crbug.com/531575122): Move passing these values to each frame to
  // TopChromeWebUIController, since the omnibox popup does the same and other
  // Top Chrome WebUIs will.
  static base::DictValue GetProfileLoadTimeData(Profile* profile);

 private:
  // content::WebUIController:
  void WebUIRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;

  // organizer_panel::mojom::ForeignTabsPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<organizer_panel::mojom::ForeignTabsPageHandler>
          receiver) override;

  // organizer_panel::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<organizer_panel::mojom::Page> page,
      mojo::PendingReceiver<organizer_panel::mojom::PageHandler> receiver)
      override;

  // tab_search::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<tab_search::mojom::Page> page,
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) override;

  // organizer_panel::mojom::TabGroupsOrganizerPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<organizer_panel::mojom::TabGroupsOrganizerPage> page,
      mojo::PendingReceiver<
          organizer_panel::mojom::TabGroupsOrganizerPageHandler> receiver)
      override;

  std::unique_ptr<ForeignTabsPageHandler> foreign_tabs_page_handler_;
  std::unique_ptr<OrganizerPanelPageHandler> organizer_panel_page_handler_;
  std::unique_ptr<TabSearchPageHandler> page_handler_;
  std::unique_ptr<SearchHandler> search_handler_;
  std::unique_ptr<TabGroupsOrganizerPageHandler>
      tab_groups_organizer_page_handler_;
  mojo::Receiver<organizer_panel::mojom::ForeignTabsPageHandlerFactory>
      foreign_tabs_page_factory_receiver_{this};
  mojo::Receiver<organizer_panel::mojom::PageHandlerFactory>
      organizer_panel_page_factory_receiver_{this};
  mojo::Receiver<tab_search::mojom::PageHandlerFactory> page_factory_receiver_{
      this};
  mojo::Receiver<organizer_panel::mojom::TabGroupsOrganizerPageHandlerFactory>
      tab_groups_page_factory_receiver_{this};

  WebuiLoadTimer webui_load_timer_;

  // Tracks the duration between WebUI construction and PageHandler creation.
  std::optional<base::ElapsedTimer> page_handler_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_UI_H_
