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
#include "components/content_settings/core/common/content_settings.h"
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
struct AutocompleteMatch;

// LINT.IfChange(OmniboxInlineLocationSuggestionShown)
enum class OmniboxInlineLocationSuggestionShown {
  // The suggestion could not be shown because there was no eligible suggestion
  // found from which to create a duplicate inline location suggestion.
  kNoEligibleSuggestionFound = 0,
  // There was a parent suggestion found, but no inline location suggestion was
  // shown.
  kOnlyParentSuggestionShown = 1,
  // An inline location suggestion was shown.
  kLocationSuggestionShown = 2,
  // Max value.
  kMaxValue = kLocationSuggestionShown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:OmniboxInlineLocationSuggestionShown)

// LINT.IfChange(GeolocationHeaderPrimeLocationOutcome)
enum class GeolocationHeaderPrimeLocationOutcome {
  kNotTriedAlreadyBound = 0,
  kNotTriedNoDefaultProvider = 1,
  kNotTriedProviderDoesNotAcceptHeader = 2,
  kNotTriedInvalidUrlOrInsecure = 3,
  kNotTriedPermissionStatusMismatch = 4,
  kNotTriedCachedLocationFresh = 5,
  kTriedFailedConnection = 6,
  kTriedQueryCachedPosition = 7,
  kTriedQueryNextPosition = 8,
  kMaxValue = kTriedQueryNextPosition,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:GeolocationHeaderPrimeLocationOutcome)

// LINT.IfChange(GeolocationHeaderGetLocationOutcome)
enum class GeolocationHeaderGetLocationOutcome {
  kSuccess = 0,
  kNoCachedLocation = 1,
  kPermissionStateMismatch = 2,
  kInsecureConnection = 3,
  kIneligibleUrl = 4,
  kHeaderGranularityMismatch = 5,
  kMaxValue = kHeaderGranularityMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:GeolocationHeaderGetLocationOutcome)

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

  // Returns the accuracy level of the current cached position if a fresh
  // location is available. Otherwise, returns std::nullopt.
  std::optional<GeolocationAccuracy> GetCachedLocationAccuracy() const;

  // Returns the serialized X-Geo header if a valid, fresh location is
  // available and the url matches the DSE. Otherwise, returns std::nullopt.
  // - `for_automatic_sending` is true when this is called for search matches
  //    that will 'invisibly' add geo header. So it returns the header only if
  //    site level permissions are explicitly granted (ALLOW).
  // - `for_automatic_sending` is false when this is called for search matches
  //    that will explicitly warn the user a geo header is included. So it
  //    returns the header only if site level permissions are not explicitly
  //    granted (ASK/DENY). Showing a location match even when the permission is
  //    DENY is ok because the match is non-intrusive and is not allowed to be
  //    default. This behavior is in line with similar features involving users
  //    proactively initiating some capability access: geolocation, usermedia,
  //    install.
  std::optional<std::string> GetLocationHeader(const GURL& url,
                                               bool for_automatic_sending);

  // Records metrics about when the inline location suggestion is shown and
  // clicked.
  void RecordInlineLocationSuggestionShown(
      OmniboxInlineLocationSuggestionShown shown_state,
      size_t match_index) const;
  void MaybeRecordInlineLocationSuggestionClicked(
      const AutocompleteMatch& match) const;

  // Returns true if the given URL is eligible for the X-Geo header.
  bool IsUrlEligibleForLocationHeader(const GURL& url) const;

 private:
  friend class GeolocationHeaderServiceTestApi;

  // Returns true if both the site-level and OS-level geolocation permissions
  // are granted for the given URL.
  bool IsAllowedByPermission(const GURL& url) const;
  bool HasPrecisePermission(const GURL& url) const;
  bool HasDeviceLocationPermission(GeolocationAccuracy accuracy) const;

  // Encapsulates the logic to connect to the device geolocation service.
  bool EnsureGeolocationServiceConnection(const GURL& requesting_url,
                                          bool use_cache_only = false);

  void OnLocationUpdate(device::mojom::GeopositionResultPtr result);

  PermissionSetting GetPermissionSetting(const GURL& url) const;
  std::optional<PermissionSetting> GetDSEPermissionSetting() const;

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
