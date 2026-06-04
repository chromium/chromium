// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_UI_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom-forward.h"
#include "components/browser_apis/browser_controls/browser_controls_api_data_model.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/extensions_bar.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"

class CommandUpdater;

namespace user_education {
class HelpBubbleHandler;
}  // namespace user_education

// The webui controller for the webui toolbar. This class has a two part
// initialization. The controller is not ready to use until after
// Init() is called.
class WebUIToolbarUI : public TopChromeWebUIController,
                       public help_bubble::mojom::HelpBubbleHandlerFactory,
                       public extensions_bar::mojom::PageHandlerFactory {
 public:
  // Provides dependencies to this controller during init.
  class DependencyProvider {
   public:
    // Cannot be null.
    virtual browser_controls_api::BrowserControlsService::
        BrowserControlsServiceDelegate*
        GetBrowserControlsDelegate() = 0;
    // Cannot be null.
    virtual toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
    GetToolbarUIServiceDelegate() = 0;
    // Cannot be null.
    virtual std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
    GetNavigationControlsStateFetcher() = 0;
    // Cannot be null.
    virtual std::unique_ptr<toolbar_ui_api::IconTableFetcher>
    GetIconTableFetcher() = 0;
    // Cannot be null.
    virtual CommandUpdater* GetCommandUpdater() = 0;
  };

  explicit WebUIToolbarUI(content::WebUI* web_ui);
  WebUIToolbarUI(const WebUIToolbarUI&) = delete;
  WebUIToolbarUI& operator=(const WebUIToolbarUI&) = delete;
  ~WebUIToolbarUI() override;

  static constexpr std::string_view GetWebUIName() { return "WebUIToolbar"; }

  void BindInterface(
      mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsService>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService> receiver);

  void BindInterface(
      mojo::PendingReceiver<extensions_bar::mojom::PageHandlerFactory>
          receiver);

  // Implements support for help bubbles (IPH, tutorials, etc.) in settings
  // pages.
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          receiver);

  void OnNavigationControlsStateChanged(
      const toolbar_ui_api::mojom::NavigationControlsState& state);
  void OnFocusRequested(toolbar_ui_api::mojom::FocusRequestTarget target);

  // The |depdency_provider| is expected to outlive this class.
  void Init(DependencyProvider* dependency_provider);

  // TopChromeWebUIController:
  // The controller uses `requesting_origin` to:
  // 1. Decide which resources to expose, e.g. only expose "chrome://theme"
  //    resources to trusted "chrome://" origins.
  // 2. Generate correct CORS headers. Since resources added here often belong
  //    to a different origin than the page loading them, they need a CORS header
  //    that explicitly allow `current_origin`.
  void PopulateLocalResourceLoaderConfig(
      blink::mojom::LocalResourceLoaderConfig* config,
      const url::Origin& requesting_origin) override;

  void WebUIRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  // extensions_bar::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<extensions_bar::mojom::Page> page,
      mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver)
      override;

  content::WebUIController::DisplayDisposition GetDisplayDisposition()
      const override;

  // Returns the list of known element identifiers. These elements are HTML
  // elements tracked by ui/webui/tracked_element. Used for anchoring secondary
  // UIs.
  static const std::vector<ui::ElementIdentifier> GetKnownElementIdentifiers();

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarUITest,
                           BindInterfaceBrowserControlsService);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarUITest, BindInterfaceToolbarUIService);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarUITest, CreateBrowserControlsService);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarUITest, CreateToolbarUIService);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarUITest,
                           CreateBrowserControlsService_NullCommandUpdater);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarUITest,
                           CreateToolbarUIService_NullCommandUpdater);

  void InitBrowserControlsService(DependencyProvider& dependency_provider);
  void InitToolbarUIService(DependencyProvider& dependency_provider);

  std::unique_ptr<browser_controls_api::BrowserControlsService>
      browser_controls_service_;
  std::unique_ptr<toolbar_ui_api::ToolbarUIService> toolbar_ui_service_;

  /////////////////////////////////////////////////////////////////////////////
  // There's a subtle edge case for WebUI toolbar, because it's hosted at the
  // top level. Ownership of the controller transfers from navigation to the
  // view. Before that happens, it is possible for the browser window to get
  // destroyed. At which point, it is possible for certain deps to become
  // destroyed. This makes state management tricky.
  //
  // To simplify state management, we use mojo pipes to allow the browser
  // and WebUI to connect. We allocate a complete mojo pipe at the
  // construction of the object. Either end (browser and webui) may connect
  // to their respective ends of the pipe at any time. This way, we do not
  // need to make any sort of assumption about the state at either end.
  //
  // We will need to hold onto the both ends of the pipe at the start.
  mojo::PendingRemote<toolbar_ui_api::mojom::ToolbarUIService>
      toolbar_channel_client_end_;
  mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService>
      toolbar_channel_service_end_;

  mojo::PendingRemote<browser_controls_api::mojom::BrowserControlsService>
      browser_controls_channel_client_end_;
  mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsService>
      browser_controls_channel_service_end_;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_service_{this};

  mojo::Receiver<extensions_bar::mojom::PageHandlerFactory>
      extensions_bar_page_factory_receiver_{this};

  /////////////////////////////////////////////////////////////////////////////

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class WebUIToolbarConfig : public DefaultTopChromeWebUIConfig<WebUIToolbarUI> {
 public:
  WebUIToolbarConfig();
  // DefaultTopChromeWebUIConfig overrides:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldKeepVisibleUntilFirstVisuallyNonEmptyPaint() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_UI_H_
