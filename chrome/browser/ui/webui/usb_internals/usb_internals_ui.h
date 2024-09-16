// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom-forward.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class UsbInternalsUI;
class UsbInternalsPageHandler;

class UsbInternalsUIConfig
    : public content::DefaultWebUIConfig<UsbInternalsUI> {
 public:
  UsbInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIUsbInternalsHost) {}
};

// The WebUI for chrome://usb-internals.
class UsbInternalsUI : public ui::MojoWebUIController {
 public:
  explicit UsbInternalsUI(content::WebUI* web_ui);

  UsbInternalsUI(const UsbInternalsUI&) = delete;
  UsbInternalsUI& operator=(const UsbInternalsUI&) = delete;

  ~UsbInternalsUI() override;

  // Instantiates the implementor of the mojom::UsbInternalsPageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver);

 private:
  std::unique_ptr<UsbInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_UI_H_
