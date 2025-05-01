// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/common/webui_url_constants.h"
#include "components/webui/chrome_urls/mojom/chrome_urls.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}

class Profile;

namespace chrome_urls {
class ChromeUrlsHandler;
class ChromeUrlsUI;

// chrome://chrome-urls. Note that HandleChromeAboutAndChromeSyncRewrite()
// rewrites chrome://about -> chrome://chrome-urls.
class ChromeUrlsUIConfig : public content::DefaultWebUIConfig<ChromeUrlsUI> {
 public:
  ChromeUrlsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIChromeURLsHost) {}
};

// The Web UI controller for the chrome://chrome-urls page.
class ChromeUrlsUI : public ui::MojoWebUIController,
                     public chrome_urls::mojom::PageHandlerFactory {
 public:
  explicit ChromeUrlsUI(content::WebUI* web_ui);
  ~ChromeUrlsUI() override;

  // Instantiates the implementor of the
  // chrome_urls::mojom::PageHandlerFactory mojo interface.
  void BindInterface(
      mojo::PendingReceiver<chrome_urls::mojom::PageHandlerFactory> receiver);

  ChromeUrlsUI(const ChromeUrlsUI&) = delete;
  ChromeUrlsUI& operator=(const ChromeUrlsUI&) = delete;

 private:
  // chrome_urls::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<chrome_urls::mojom::Page> page,
      mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver) override;

  std::unique_ptr<ChromeUrlsHandler> page_handler_;
  mojo::Receiver<chrome_urls::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  raw_ptr<Profile> profile_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chrome_urls

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_UI_H_
