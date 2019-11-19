// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"

#include <utility>

#include "content/public/browser/system_connector.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

UsbInternalsPageHandler::UsbInternalsPageHandler(
    mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

UsbInternalsPageHandler::~UsbInternalsPageHandler() {}

void UsbInternalsPageHandler::BindTestInterface(
    mojo::PendingReceiver<device::mojom::UsbDeviceManagerTest> receiver) {
  // Forward the request to the DeviceService.
  content::GetSystemConnector()->Connect(device::mojom::kServiceName,
                                         std::move(receiver));
}

void UsbInternalsPageHandler::BindUsbDeviceManagerInterface(
    mojo::PendingReceiver<device::mojom::UsbDeviceManager> receiver) {
  // Forward the request to the DeviceService.
  content::GetSystemConnector()->Connect(device::mojom::kServiceName,
                                         std::move(receiver));
}
