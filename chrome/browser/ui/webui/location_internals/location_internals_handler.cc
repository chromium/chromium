// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/location_internals/location_internals_handler.h"

#include <utility>

#include "content/public/browser/device_service.h"

LocationInternalsHandler::LocationInternalsHandler(
    mojo::PendingReceiver<mojom::LocationInternalsHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

LocationInternalsHandler::~LocationInternalsHandler() = default;

void LocationInternalsHandler::BindInternalsInterface(
    mojo::PendingReceiver<device::mojom::GeolocationInternals> receiver) {
  // Forward the request to the DeviceService.
  content::GetDeviceService().BindGeolocationInternals(std::move(receiver));
}
