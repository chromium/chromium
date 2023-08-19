// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_HANDLER_H_

#include "chrome/browser/ui/webui/location_internals/location_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"

// Handles API requests from chrome://location-internals page by implementing
// mojom::LocationInternalsHandler.
class LocationInternalsHandler : public mojom::LocationInternalsHandler {
 public:
  explicit LocationInternalsHandler(
      mojo::PendingReceiver<mojom::LocationInternalsHandler> receiver);

  LocationInternalsHandler(const LocationInternalsHandler&) = delete;
  LocationInternalsHandler& operator=(const LocationInternalsHandler&) = delete;

  ~LocationInternalsHandler() override;

  // mojom::LocationInternalsHandler:
  void BindInternalsInterface(
      mojo::PendingReceiver<device::mojom::GeolocationInternals> receiver)
      override;

 private:
  mojo::Receiver<mojom::LocationInternalsHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_HANDLER_H_
