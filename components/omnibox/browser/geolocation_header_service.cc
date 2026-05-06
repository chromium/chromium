// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/geolocation_header_service.h"

#include "base/base64url.h"
#include "base/strings/strcat.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/proto/partner_location_descriptor.pb.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/permissions/permissions_client.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/location/android/location_settings.h"
#endif  // BUILDFLAG(IS_ANDROID)

#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"

namespace {
constexpr base::TimeDelta kMaxLocationAgeForPriming = base::Minutes(5);
constexpr base::TimeDelta kMaxLocationAgeForHeader = base::Hours(24);
// The "w " prefix identifies the subsequent string as a Base64-encoded proto
// as defined by the X-Geo protocol.
constexpr std::string_view kLocationProtoPrefix = "w ";

// This is a duplicate of the logic in GeolocationHeader.java, used as we
// transition X-Geo logic from Android-specific to platform-agnostic C++. Once
// the transition is complete, the Java implementation will be removed.
std::optional<std::string> SerializeXGeoHeader(
    const device::mojom::Geoposition& position) {
  omnibox::LocationDescriptor descriptor;
  descriptor.set_role(omnibox::CURRENT_LOCATION);
  descriptor.set_producer(omnibox::DEVICE_LOCATION);
  descriptor.set_timestamp(
      (position.timestamp - base::Time::UnixEpoch()).InMicroseconds());

  omnibox::LatLng* latlng = descriptor.mutable_latlng();
  latlng->set_latitude_e7(static_cast<int32_t>(position.latitude * 1e7));
  latlng->set_longitude_e7(static_cast<int32_t>(position.longitude * 1e7));
  descriptor.set_radius(position.accuracy * 1000);

  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxXGeoPermissionGranularity)) {
    descriptor.set_permission_granularity(
        position.is_precise ? omnibox::PERMISSION_GRANULARITY_FINE
                            : omnibox::PERMISSION_GRANULARITY_COARSE);
  }

  std::string serialized_proto;
  if (!descriptor.SerializeToString(&serialized_proto)) {
    return std::nullopt;
  }

  // Base64UrlEncode directly translates to Base64.URL_SAFE. INCLUDE_PADDING
  // matches NO_WRAP | URL_SAFE's output on Android.
  std::string encoded;
  base::Base64UrlEncode(serialized_proto,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING, &encoded);
  return base::StrCat({kLocationProtoPrefix, encoded});
}

}  // namespace

GeolocationHeaderService::GeolocationHeaderService(
    HostContentSettingsMap* settings_map,
    TemplateURLService* template_url_service
#if BUILDFLAG(IS_ANDROID)
    ,
    std::unique_ptr<LocationSettings> location_settings
#endif
    )
    : settings_map_(settings_map),
      template_url_service_(template_url_service)
#if BUILDFLAG(IS_ANDROID)
      ,
      location_settings_(std::move(location_settings))
#endif
{
  CHECK(settings_map_);
}

GeolocationHeaderService::~GeolocationHeaderService() = default;

void GeolocationHeaderService::Shutdown() {
  template_url_service_ = nullptr;
  geolocation_.reset();
  geolocation_context_.reset();
  last_position_.reset();
}

void GeolocationHeaderService::PrimeLocation() {
  if (geolocation_.is_bound() || !template_url_service_) {
    return;
  }

  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider) {
    return;
  }

  GURL requesting_url = default_provider->GenerateSearchURL(
      template_url_service_->search_terms_data());

  bool is_ills_enabled =
      base::FeatureList::IsEnabled(omnibox::kInlineLocationSignaling);

  if (!requesting_url.is_valid() ||
      !requesting_url.SchemeIs(url::kHttpsScheme) ||
      (!IsAllowedByPermission(requesting_url) && !is_ills_enabled)) {
    last_position_.reset();
    return;
  }

  // If the location is fresh there is no need to query for a new one.
  if (HasCachedLocation()) {
    base::TimeDelta age = location_age_for_testing_.value_or(
        base::Time::Now() - last_position_->timestamp);
    if (age <= kMaxLocationAgeForPriming) {
      return;
    }
  }

  bool use_cache_only =
      is_ills_enabled && !IsAllowedByPermission(requesting_url);

  if (!EnsureGeolocationServiceConnection(requesting_url, use_cache_only)) {
    return;
  }

  auto callback = base::BindOnce(&GeolocationHeaderService::OnLocationUpdate,
                                 weak_factory_.GetWeakPtr());
  if (use_cache_only) {
    geolocation_->QueryCachedPosition(std::move(callback));
  } else {
    geolocation_->QueryNextPosition(std::move(callback));
  }
}

bool GeolocationHeaderService::HasCachedLocation() const {
  if (!last_position_) {
    return false;
  }
  if (location_age_for_testing_.has_value()) {
    return location_age_for_testing_.value() <= kMaxLocationAgeForHeader;
  }
  return base::Time::Now() - last_position_->timestamp <=
         kMaxLocationAgeForHeader;
}

std::optional<std::string> GeolocationHeaderService::GetLocationHeader(
    const GURL& url) {
  if (!url.SchemeIs(url::kHttpsScheme) || !HasCachedLocation() ||
      !IsUrlEligibleForLocationHeader(url)) {
    return std::nullopt;
  }

  if (!IsAllowedByPermission(url) ||
      (last_position_->is_precise && !HasPrecisePermission(url))) {
    last_position_.reset();
    geolocation_.reset();
    return std::nullopt;
  }

  return SerializeXGeoHeader(*last_position_);
}

bool GeolocationHeaderService::IsAllowedByPermission(const GURL& url) const {
  if (!HasDeviceLocationPermission(GeolocationAccuracy::kApproximate)) {
    return false;
  }

  PermissionSetting setting = settings_map_->GetPermissionSetting(
      url, url, content_settings::GeolocationContentSettingsType());

  return content_settings::PermissionSettingsRegistry::GetInstance()
      ->Get(content_settings::GeolocationContentSettingsType())
      ->delegate()
      .IsAnyPermissionAllowed(setting);
}

bool GeolocationHeaderService::HasPrecisePermission(const GURL& url) const {
  if (!HasDeviceLocationPermission(GeolocationAccuracy::kPrecise)) {
    return false;
  }

  PermissionSetting setting = settings_map_->GetPermissionSetting(
      url, url, content_settings::GeolocationContentSettingsType());

  return std::visit(absl::Overload(
                        [](const GeolocationSetting& geo_setting) {
                          return geo_setting.precise ==
                                 PermissionOption::kAllowed;
                        },
                        [](ContentSetting content_setting) {
                          return content_setting == CONTENT_SETTING_ALLOW;
                        }),
                    setting);
}

bool GeolocationHeaderService::HasDeviceLocationPermission(
    GeolocationAccuracy accuracy) const {
#if BUILDFLAG(IS_ANDROID)
  if (!location_settings_) {
    return true;
  }
  return accuracy == GeolocationAccuracy::kPrecise
             ? location_settings_->HasAndroidFineLocationPermission()
             : location_settings_->HasAndroidLocationPermission();
#else
  return permissions::PermissionsClient::Get()->HasDevicePermission(
      ContentSettingsType::GEOLOCATION);
#endif
}

bool GeolocationHeaderService::IsUrlEligibleForLocationHeader(
    const GURL& url) const {
  if (!template_url_service_) {
    return false;
  }
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider) {
    return false;
  }

  // 1. If the URL is a perfect match for the user's Default Search Engine
  //    template (e.g., it contains the correct search terms parameter), it is
  //    eligible.
  if (default_provider->IsSearchURL(
          url, template_url_service_->search_terms_data())) {
    return true;
  }

  // 2. If it is not a perfect template match, we provide a fallback exclusively
  //    for Google search properties. This is necessary because valid Google
  //    searches can occur on non-standard paths (e.g. /webhp) or use hash
  //    fragments (e.g. /#q=) which fail the strict IsSearchURL check but still
  //    represent actual search queries.
  bool is_google_dse =
      default_provider->GetEngineType(
          template_url_service_->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  GURL dse_url = default_provider->GenerateSearchURL(
      template_url_service_->search_terms_data());

  bool is_dse_origin =
      url::Origin::Create(url).IsSameOriginWith(url::Origin::Create(dse_url));

  // The fallback requires the URL to be a recognized Google search URL and to
  // exactly match the DSE's origin. This prevents leaking location to
  // non-search Google properties (like google.com/maps) or cross-TLD
  // navigations (like google.ca when the DSE is google.com).
  return is_google_dse && is_dse_origin && google_util::IsGoogleSearchUrl(url);
}

bool GeolocationHeaderService::EnsureGeolocationServiceConnection(
    const GURL& requesting_url,
    bool use_cache_only) {
  if (geolocation_.is_bound()) {
    return true;
  }

  if (!geolocation_context_.is_bound()) {
    content::GetDeviceService().BindGeolocationContext(
        geolocation_context_.BindNewPipeAndPassReceiver());
  }

  if (!geolocation_context_.is_bound()) {
    return false;
  }

  // We pass the requesting_url to the Device Service so that the OS level
  // location prompt can attribute the location request to the correct origin.
  bool has_precise = HasPrecisePermission(requesting_url);
  geolocation_context_->BindGeolocation(
      geolocation_.BindNewPipeAndPassReceiver(), requesting_url,
      device::mojom::GeolocationClientId::kOmnibox, has_precise);

  if (!use_cache_only) {
    geolocation_->SetHighAccuracyHint(has_precise);
  }

  geolocation_.set_disconnect_handler(base::BindOnce(
      [](base::WeakPtr<GeolocationHeaderService> service) {
        if (service) {
          service->geolocation_.reset();
        }
      },
      weak_factory_.GetWeakPtr()));

  return geolocation_.is_bound();
}

void GeolocationHeaderService::OnLocationUpdate(
    device::mojom::GeopositionResultPtr result) {
  // We only want to query the location once (which returns the cached/latest
  // known position). Resetting the remote immediately destroys the underlying
  // GeolocationImpl and stops it from continuously watching for location
  // updates.
  geolocation_.reset();

  if (result->is_position()) {
    last_position_ = std::move(result->get_position());
  }
}
