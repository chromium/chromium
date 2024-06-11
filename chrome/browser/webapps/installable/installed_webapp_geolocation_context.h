// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_GEOLOCATION_CONTEXT_H_
#define CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_GEOLOCATION_CONTEXT_H_

#include <memory>
#include <vector>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "url/origin.h"

class InstalledWebappGeolocationBridge;

// Provides information to a set of InstalledWebappGeolocationBridge instances
// that are associated with a given context. Notably, allows pausing and
// resuming geolocation on these instances.
class InstalledWebappGeolocationContext
    : public device::mojom::GeolocationContext {
 public:
  InstalledWebappGeolocationContext();
  InstalledWebappGeolocationContext(const InstalledWebappGeolocationContext&) =
      delete;
  InstalledWebappGeolocationContext& operator=(
      const InstalledWebappGeolocationContext&) = delete;
  ~InstalledWebappGeolocationContext() override;

  // mojom::GeolocationContext implementation:
  void BindGeolocation(
      mojo::PendingReceiver<device::mojom::Geolocation> receiver,
      const GURL& requesting_url,
      device::mojom::GeolocationClientId client_id) override;
  void OnPermissionRevoked(const url::Origin& origin) override;
  void SetOverride(device::mojom::GeopositionResultPtr result) override;
  void ClearOverride() override;

  // Called when a InstalledWebappGeolocationBridge has a connection error.
  // After this call, it is no longer safe to access |impl|.
  void OnConnectionError(InstalledWebappGeolocationBridge* impl);

 private:
  std::vector<std::unique_ptr<InstalledWebappGeolocationBridge>> impls_;

  device::mojom::GeopositionResultPtr geoposition_override_;
};

#endif  // CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_GEOLOCATION_CONTEXT_H_
