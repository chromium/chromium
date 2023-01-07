// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"

#include <utility>

#include "content/public/browser/device_service.h"

UsbInternalsPageHandler::UsbInternalsPageHandler(
    mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

UsbInternalsPageHandler::~UsbInternalsPageHandler() {}

void UsbInternalsPageHandler::BindTestInterface(
    mojo::PendingReceiver<device::mojom::UsbDeviceManagerTest> receiver) {
  // Forward the request to the DeviceService.
  content::GetDeviceService().BindUsbDeviceManagerTest(std::move(receiver));
}

void UsbInternalsPageHandler::BindUsbDeviceManagerInterface(
    mojo::PendingReceiver<device::mojom::UsbDeviceManager> receiver) {
  // Forward the request to the DeviceService.
  content::GetDeviceService().BindUsbDeviceManager(std::move(receiver));
}
