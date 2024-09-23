// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UNTRUSTED_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/common/compose/compose.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

class ComposeUntrustedUI;

class ComposeUIUntrustedConfig
    : public DefaultTopChromeWebUIConfig<ComposeUntrustedUI> {
 public:
  ComposeUIUntrustedConfig();

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldAutoResizeHost() override;
};

// TODO(b/317056725): update mojom to reflect that the page is untrusted.
class ComposeUntrustedUI
    : public UntrustedTopChromeWebUIController,
      public compose::mojom::ComposeSessionUntrustedPageHandlerFactory {
 public:
  explicit ComposeUntrustedUI(content::WebUI* web_ui);

  ComposeUntrustedUI(const ComposeUntrustedUI&) = delete;
  ComposeUntrustedUI& operator=(const ComposeUntrustedUI&) = delete;
  ~ComposeUntrustedUI() override;
  void BindInterface(
      mojo::PendingReceiver<
          compose::mojom::ComposeSessionUntrustedPageHandlerFactory> factory);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  void set_triggering_web_contents(content::WebContents* web_contents) {
    triggering_web_contents_ = web_contents->GetWeakPtr();
  }

  static constexpr std::string GetWebUIName() { return "Compose"; }

 private:
  void CreateComposeSessionUntrustedPageHandler(
      mojo::PendingReceiver<compose::mojom::ComposeClientUntrustedPageHandler>
          close_handler,
      mojo::PendingReceiver<compose::mojom::ComposeSessionUntrustedPageHandler>
          handler,
      mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog> dialog)
      override;
  mojo::Receiver<compose::mojom::ComposeSessionUntrustedPageHandlerFactory>
      session_handler_factory_{this};

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  base::WeakPtr<content::WebContents> triggering_web_contents_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UNTRUSTED_UI_H_
