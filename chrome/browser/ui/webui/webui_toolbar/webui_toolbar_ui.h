// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom-forward.h"
#include "components/browser_apis/browser_controls/browser_controls_api_data_model.mojom.h"
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
   //
class CommandUpdater;

// The webui controller for the webui toolbar. This class has a two part
// initialization. The controller is not ready to use until after
// Init() is called.
class WebUIToolbarUI : public TopChromeWebUIController {
 public:
  // Provides dependencies to this controller during init.
  class DependencyProvider {
   public:
    virtual browser_controls_api::BrowserControlsService::Delegate*
    GetDelegate() = 0;
    virtual std::unique_ptr<
        browser_controls_api::NavigationControlsStateFetcher>
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
      mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
          receiver);

  void OnNavigationControlsStateChanged(
      browser_controls_api::mojom::NavigationControlsStatePtr state);

  // The |depdency_provider| is expected to outlive this class.
  void Init(DependencyProvider* dependency_provider);

  browser_controls_api::BrowserControlsService*
  browser_controls_service_for_testing();

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
  CommandUpdater* GetCommandUpdater() const;

  // Returns the list of known element identifiers. These elements are HTML
  // elements tracked by ui/webui/tracked_element. Used for anchoring secondary
  // UIs.
  const std::vector<ui::ElementIdentifier> GetKnownElementIdentifiers() const;

  std::unique_ptr<browser_controls_api::BrowserControlsService>
      browser_controls_service_;
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
