// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace on_device_internals {

class OnDeviceInternalsUI;

// WebUIConfig for chrome://on-device-internals
class OnDeviceInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<OnDeviceInternalsUI> {
 public:
  OnDeviceInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIOnDeviceInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// A dev UI for testing the OnDeviceModelService.
class OnDeviceInternalsUI : public ui::MojoWebUIController,
                            public mojom::PageHandlerFactory {
 public:
  explicit OnDeviceInternalsUI(content::WebUI* web_ui);
  ~OnDeviceInternalsUI() override;

  OnDeviceInternalsUI(const OnDeviceInternalsUI&) = delete;
  OnDeviceInternalsUI& operator=(const OnDeviceInternalsUI&) = delete;

  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

 private:
  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> receiver) override;

  std::unique_ptr<mojom::PageHandler> page_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace on_device_internals

#endif  // CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_
