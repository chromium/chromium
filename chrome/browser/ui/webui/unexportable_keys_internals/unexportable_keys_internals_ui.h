// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class UnexportableKeysInternalsUI;
class UnexportableKeysInternalsHandler;

// The WebUIConfig for chrome://unexportable-keys-internals.
class UnexportableKeysInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<UnexportableKeysInternalsUI> {
 public:
  UnexportableKeysInternalsUIConfig()
      : content::DefaultInternalWebUIConfig<UnexportableKeysInternalsUI>(
            chrome::kChromeUIUnexportableKeysInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUIController for chrome://unexportable-keys-internals.
class UnexportableKeysInternalsUI
    : public ui::MojoWebUIController,
      public unexportable_keys_internals::mojom::PageHandlerFactory {
 public:
  explicit UnexportableKeysInternalsUI(content::WebUI* web_ui);
  UnexportableKeysInternalsUI(const UnexportableKeysInternalsUI&) = delete;
  UnexportableKeysInternalsUI& operator=(const UnexportableKeysInternalsUI&) =
      delete;
  ~UnexportableKeysInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          unexportable_keys_internals::mojom::PageHandlerFactory> receiver);

 private:
  // unexportable_keys_internals::mojom::PageHandlerFactory:
  void CreateUnexportableKeysInternalsHandler(
      mojo::PendingRemote<unexportable_keys_internals::mojom::Page> page,
      mojo::PendingReceiver<unexportable_keys_internals::mojom::PageHandler>
          receiver) override;

  std::unique_ptr<UnexportableKeysInternalsHandler> page_handler_;

  mojo::Receiver<unexportable_keys_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_UI_H_
