// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class UsbInternalsPageHandler;

// The WebUI for chrome://usb-internals.
class UsbInternalsUI : public ui::MojoWebUIController {
 public:
  explicit UsbInternalsUI(content::WebUI* web_ui);
  ~UsbInternalsUI() override;

 private:
  void BindUsbInternalsPageHandler(
      mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver);

  std::unique_ptr<UsbInternalsPageHandler> page_handler_;

  DISALLOW_COPY_AND_ASSIGN(UsbInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_UI_H_
