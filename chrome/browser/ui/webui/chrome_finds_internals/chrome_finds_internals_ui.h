// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_INTERNALS_UI_H_

#include "build/build_config.h"
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chrome_finds_internals {

class ChromeFindsInternalsUI;

class ChromeFindsInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<ChromeFindsInternalsUI> {
 public:
  ChromeFindsInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIChromeFindsInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://chrome-finds-internals.
class ChromeFindsInternalsUI : public ui::MojoWebUIController,
                               public mojom::PageHandlerFactory {
 public:
  explicit ChromeFindsInternalsUI(content::WebUI* web_ui);

  ChromeFindsInternalsUI(const ChromeFindsInternalsUI&) = delete;
  ChromeFindsInternalsUI& operator=(const ChromeFindsInternalsUI&) = delete;

  ~ChromeFindsInternalsUI() override;

  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

 private:
  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> receiver) override;

  std::unique_ptr<mojom::PageHandler> page_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chrome_finds_internals

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_INTERNALS_UI_H_
