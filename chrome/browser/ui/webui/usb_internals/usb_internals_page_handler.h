// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_test.mojom.h"

class UsbInternalsPageHandler : public mojom::UsbInternalsPageHandler {
 public:
  explicit UsbInternalsPageHandler(
      mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver);
  ~UsbInternalsPageHandler() override;

  void BindUsbDeviceManagerInterface(
      mojo::PendingReceiver<device::mojom::UsbDeviceManager> receiver) override;

  void BindTestInterface(
      mojo::PendingReceiver<device::mojom::UsbDeviceManagerTest> receiver)
      override;

 private:
  mojo::Receiver<mojom::UsbInternalsPageHandler> receiver_;

  DISALLOW_COPY_AND_ASSIGN(UsbInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_USB_INTERNALS_USB_INTERNALS_PAGE_HANDLER_H_
