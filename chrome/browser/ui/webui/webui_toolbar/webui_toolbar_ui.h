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
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace gfx {
class FontList;
}  // namespace gfx

class CommandUpdater;

// The webui controller for the webui toolbar. This class has a two part
// initialization. The controller is not ready to use until after
// Init() is called.
class WebUIToolbarUI : public TopChromeWebUIController {
 public:
  // Provides dependencies to this controller during init.
  class DependencyProvider {
   public:
    virtual browser_controls_api::BrowserControlsService::
        BrowserControlsServiceDelegate*
        GetBrowserControlsDelegate() = 0;
    virtual toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
    GetToolbarUIServiceDelegate() = 0;
    virtual std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
    GetNavigationControlsStateFetcher() = 0;
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
      mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
          receiver);

  void OnNavigationControlsStateChanged(
      toolbar_ui_api::mojom::NavigationControlsStatePtr state);

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

  // Adds a set of variables describing `font` into `source`. Assumes
  // `font` has one entry.
  //
  // The variables are:
  // ${prefix}Family --- the font name, as string. May need escaping before use!
  // ${prefix}Size --- font height in pixels, as integer.
  // ${prefix}Weight --- font weight, as integer.
  static void AddFontVariables(std::string_view prefix,
                               const gfx::FontList& font,
                               content::WebUIDataSource* source);

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
  CommandUpdater* GetCommandUpdater() const;

  // Returns the list of known element identifiers. These elements are HTML
  // elements tracked by ui/webui/tracked_element. Used for anchoring secondary
  // UIs.
  const std::vector<ui::ElementIdentifier> GetKnownElementIdentifiers() const;

  std::unique_ptr<browser_controls_api::BrowserControlsService>
      browser_controls_service_;
  std::unique_ptr<toolbar_ui_api::ToolbarUIService> toolbar_ui_service_;
  std::unique_ptr<ui::TrackedElementHandler> tracked_element_handler_;

  raw_ptr<DependencyProvider> dependency_provider_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class WebUIToolbarConfig : public DefaultTopChromeWebUIConfig<WebUIToolbarUI> {
 public:
  WebUIToolbarConfig();
  // DefaultTopChromeWebUIConfig overrides:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_UI_H_
