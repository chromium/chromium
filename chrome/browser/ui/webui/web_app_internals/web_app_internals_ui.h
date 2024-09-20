// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_UI_H_

#include "base/check_is_test.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class WebAppInternalsHandler;
class WebAppInternalsUI;

class WebAppInternalsUIConfig
    : public content::DefaultWebUIConfig<WebAppInternalsUI> {
 public:
  WebAppInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIWebAppInternalsHost) {}
};

// The WebUI for chrome://web-app-internals
class WebAppInternalsUI : public ui::MojoWebUIController {
 public:
  explicit WebAppInternalsUI(content::WebUI* web_ui);

  WebAppInternalsUI(const WebAppInternalsUI&) = delete;
  WebAppInternalsUI& operator=(const WebAppInternalsUI&) = delete;

  ~WebAppInternalsUI() override;

  // Instantiates the implementor of the mojom::WebAppInternalsHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver);

  WebAppInternalsHandler* GetHandlerForTesting() {
    CHECK_IS_TEST();
    return page_handler_.get();
  }

 private:
  std::unique_ptr<WebAppInternalsHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_UI_H_
