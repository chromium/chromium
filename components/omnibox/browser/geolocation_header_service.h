// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_GEOLOCATION_HEADER_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_GEOLOCATION_HEADER_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/location/android/location_settings.h"
#include "components/location/android/location_settings_impl.h"
#endif

class HostContentSettingsMap;
class TemplateURLService;

// A KeyedService that handles the generation of the X-Geo header for valid DSE
// navigations after checking that permissions allow for it.
class GeolocationHeaderService : public KeyedService {
 public:
  GeolocationHeaderService(HostContentSettingsMap* settings_map,
                           TemplateURLService* template_url_service
#if BUILDFLAG(IS_ANDROID)
                           ,
                           std::unique_ptr<LocationSettings> location_settings =
                               std::make_unique<LocationSettingsImpl>()
#endif
  );
  GeolocationHeaderService(const GeolocationHeaderService&) = delete;
  GeolocationHeaderService& operator=(const GeolocationHeaderService&) = delete;
  ~GeolocationHeaderService() override;

  // KeyedService:
  void Shutdown() override;

  // Starts a location request to have a fresh one ready.
  void PrimeLocation();

  // Returns true if a location is available and cached.
  bool HasCachedLocation() const;

  // Returns the serialized X-Geo header if a valid, fresh location is
  // available and the url matches the DSE. Otherwise, returns std::nullopt.
  std::optional<std::string> GetLocationHeader(const GURL& url);

  void SetLocationAgeForTesting(base::TimeDelta age) {
    location_age_for_testing_ = age;
  }

  bool is_geolocation_bound_for_testing() const {
    return geolocation_.is_bound();
  }

 private:
  // Returns true if both the site-level and OS-level geolocation permissions
  // are granted for the given URL.
  bool IsAllowedByPermission(const GURL& url) const;
  bool HasPrecisePermission(const GURL& url) const;
  bool HasDeviceLocationPermission(GeolocationAccuracy accuracy) const;
  bool IsUrlEligibleForLocationHeader(const GURL& url) const;

  // Encapsulates the logic to connect to the device geolocation service.
  bool EnsureGeolocationServiceConnection(const GURL& requesting_url,
                                          bool use_cache_only = false);

  void OnLocationUpdate(device::mojom::GeopositionResultPtr result);

  scoped_refptr<HostContentSettingsMap> settings_map_;
  raw_ptr<TemplateURLService> template_url_service_;

  mojo::Remote<device::mojom::GeolocationContext> geolocation_context_;
  mojo::Remote<device::mojom::Geolocation> geolocation_;
  device::mojom::GeopositionPtr last_position_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<LocationSettings> location_settings_;
#endif
  std::optional<base::TimeDelta> location_age_for_testing_;

  base::WeakPtrFactory<GeolocationHeaderService> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_GEOLOCATION_HEADER_SERVICE_H_
