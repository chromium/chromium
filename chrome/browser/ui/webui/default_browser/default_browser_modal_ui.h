// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/default_browser/default_browser_modal.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"

class DefaultBrowserModalUI;
class DefaultBrowserModalHandler;

namespace content {
class WebUI;
}

class DefaultBrowserModalUIConfig final
    : public DefaultTopChromeWebUIConfig<DefaultBrowserModalUI> {
 public:
  DefaultBrowserModalUIConfig();
  ~DefaultBrowserModalUIConfig() override;

  bool ShouldAutoResizeHost() override;
};

// The WebUI controller for the chrome://default-browser-modal page.
class DefaultBrowserModalUI final
    : public TopChromeWebUIController,
      public default_browser_modal::mojom::PageHandlerFactory {
 public:
  explicit DefaultBrowserModalUI(content::WebUI* web_ui);

  DefaultBrowserModalUI(const DefaultBrowserModalUI&) = delete;
  const DefaultBrowserModalUI& operator=(const DefaultBrowserModalUI&) = delete;

  ~DefaultBrowserModalUI() override;

  // default_browser_modal::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<default_browser_modal::mojom::Page> page,
      mojo::PendingReceiver<default_browser_modal::mojom::PageHandler> receiver)
      override;

  void BindInterface(
      mojo::PendingReceiver<default_browser_modal::mojom::PageHandlerFactory>
          receiver);

  static constexpr std::string_view GetWebUIName() {
    return "DefaultBrowserModal";
  }

 private:
  std::unique_ptr<DefaultBrowserModalHandler> page_handler_;
  mojo::Receiver<default_browser_modal::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_UI_H_
