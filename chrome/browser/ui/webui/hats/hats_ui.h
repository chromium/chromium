// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_

#include "chrome/browser/ui/webui/hats/hats.mojom.h"
#include "chrome/browser/ui/webui/hats/hats_page_handler.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_web_ui_controller.h"

class HatsUI;

// The configuration for the chrome-untrusted://hats page.
class HatsUIConfig : public content::DefaultWebUIConfig<HatsUI> {
 public:
  HatsUIConfig();
  ~HatsUIConfig() override = default;

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class HatsPageHandler;

class HatsUI : public ui::UntrustedWebUIController,
               public hats::mojom::PageHandlerFactory {
 public:
  explicit HatsUI(content::WebUI* web_ui);

  HatsUI(const HatsUI&) = delete;
  HatsUI& operator=(const HatsUI&) = delete;
  ~HatsUI() override;

  void SetHatsPageHandlerDelegate(HatsPageHandlerDelegate* delegate);

  void BindInterface(
      mojo::PendingReceiver<hats::mojom::PageHandlerFactory> receiver);

 private:
  // hats::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<hats::mojom::Page> page,
      mojo::PendingReceiver<hats::mojom::PageHandler> receiver) override;

  std::unique_ptr<HatsPageHandler> page_handler_;
  raw_ptr<HatsPageHandlerDelegate> page_handler_delegate_ = nullptr;

  mojo::Receiver<hats::mojom::PageHandlerFactory> page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_
