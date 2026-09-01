// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"

namespace user_education {
class HelpBubbleHandler;
}

namespace omnibox_everywhere_debug {
class OmniboxEverywhereDebugPageHandler;
}

class ComposeboxEverywhereHandler;
class MostVisitedHandler;
class MostVisitedPrefObserver;
class OmniboxEverywhereHandler;
class Profile;

namespace contextual_search {
class ContextualSearchSessionHandle;
}

class OmniboxEverywhereUI;

class OmniboxEverywhereUIConfig
    : public DefaultTopChromeWebUIConfig<OmniboxEverywhereUI> {
 public:
  OmniboxEverywhereUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUIOmniboxEverywhereHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldCrashOnJavascriptErrorInDevelopmentBuild() const override;
};

class OmniboxEverywhereUI
    : public TopChromeWebUIController,
      public composebox::mojom::PageHandlerFactory,
      public searchbox::mojom::PageHandlerFactory,
      public omnibox_everywhere_debug::mojom::PageHandlerFactory,
      public most_visited::mojom::MostVisitedPageHandlerFactory,
      public help_bubble::mojom::HelpBubbleHandlerFactory,
      public ContextualSearchboxHandler::ScreenshareDelegate,
      public ui::SimpleMenuModel::Delegate {
 public:
  explicit OmniboxEverywhereUI(content::WebUI* web_ui);
  OmniboxEverywhereUI(const OmniboxEverywhereUI&) = delete;
  OmniboxEverywhereUI& operator=(const OmniboxEverywhereUI&) = delete;
  ~OmniboxEverywhereUI() override;

  static constexpr std::string_view GetWebUIName() {
    return "OmniboxEverywhere";
  }

  // most_visited::mojom::MostVisitedPageHandlerFactory:
  void BindInterface(
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
          receiver);
  void CreatePageHandler(
      mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
      mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
          pending_page_handler) override;

  // composebox::mojom::PageHandlerFactory:
  void BindInterface(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver);
  void CreatePageHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override;

  // searchbox::mojom::PageHandlerFactory:
  void BindInterface(content::RenderFrameHost* host,
                     mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
                         pending_page_handler);
  void CreatePageHandler(
      mojo::PendingRemote<searchbox::mojom::Page> page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler> handler) override;

  // omnibox_everywhere_debug::mojom::PageHandlerFactory:
  void BindInterface(
      mojo::PendingReceiver<omnibox_everywhere_debug::mojom::PageHandlerFactory>
          receiver);
  void CreatePageHandler(
      mojo::PendingRemote<omnibox_everywhere_debug::mojom::Page> page,
      mojo::PendingReceiver<omnibox_everywhere_debug::mojom::PageHandler>
          handler) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          receiver);
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  ComposeboxEverywhereHandler* composebox_handler() {
    return composebox_handler_.get();
  }
  OmniboxEverywhereHandler* omnibox_handler() { return omnibox_handler_.get(); }

  // TODO(b/555331826): Clean up handler retrieval to avoid inspecting handler
  // instantiation state.
  // Returns the active ContextualSearchboxHandler (either composebox_handler_
  // or omnibox_handler_).
  ContextualSearchboxHandler* GetContextualSearchboxHandler();

  // ContextualSearchboxHandler::ScreenshareDelegate:
  void ShowScreenshotMenu(
      const gfx::Rect& anchor_rect,
      base::WeakPtr<ContextualSearchboxScreenshareController> controller)
      override;
  void OnScreensharePickerOpened() override;
  void OnScreensharePickerClosed() override;
  void ShowRegionSelectOverlay(const SkBitmap& screenshot,
                               const RegionCaptureSource& source,
                               RegionSelectedCallback callback) override;
  void OnScreenshotMenuClosed();

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;

  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();
  void ClearContextualSessionHandle();

 private:
  raw_ptr<Profile> profile_;

  std::unique_ptr<ComposeboxEverywhereHandler> composebox_handler_;
  std::unique_ptr<OmniboxEverywhereHandler> omnibox_handler_;
  std::unique_ptr<MostVisitedHandler> most_visited_handler_;
  std::unique_ptr<MostVisitedPrefObserver> most_visited_pref_observer_;

  std::unique_ptr<omnibox_everywhere_debug::OmniboxEverywhereDebugPageHandler>
      debug_page_handler_;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;

  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      shared_session_handle_;

  std::unique_ptr<ui::SimpleMenuModel> screenshot_menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> screenshot_menu_runner_;
  base::WeakPtr<ContextualSearchboxScreenshareController>
      active_screenshot_controller_;

  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_factory_receiver_{this};
  mojo::Receiver<most_visited::mojom::MostVisitedPageHandlerFactory>
      most_visited_page_factory_receiver_{this};
  mojo::Receiver<searchbox::mojom::PageHandlerFactory>
      searchbox_page_factory_receiver_{this};
  mojo::Receiver<omnibox_everywhere_debug::mojom::PageHandlerFactory>
      debug_page_factory_receiver_{this};
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  base::WeakPtrFactory<OmniboxEverywhereUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_H_
