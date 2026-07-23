// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/geolocation_header_service.h"

#include "base/base64url.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/autocomplete_match.h"
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
constexpr char kHistogramGetLocationOutcomePrefix[] =
    "Omnibox.GeolocationHeaderService.GetLocationOutcome.";
constexpr char kTriggerAutomaticSuffix[] = "Automatic";
constexpr char kTriggerInlineSuggestionSuffix[] = "InlineSuggestion";
constexpr char kHistogramPrimeLocationOutcome[] =
    "Omnibox.GeolocationHeaderService.PrimeLocationOutcome";

constexpr char kHistogramInlineLocationSuggestionDenyPrefix[] =
    "Omnibox.InlineLocationSuggestion.Deny";
constexpr char kHistogramInlineLocationSuggestionAskPrefix[] =
    "Omnibox.InlineLocationSuggestion.Ask";
constexpr char kHistogramInlineLocationSuggestionShownSuffix[] = ".ShownState";
constexpr char kHistogramInlineLocationSuggestionClickedSuffix[] = ".Clicked";
constexpr char kHistogramInlineLocationSuggestionParentClickedSuffix[] =
    ".ParentClicked";

constexpr char kHistogramInlineLocationSuggestionIndex[] =
    "Omnibox.InlineLocationSuggestion.Index";

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

void RecordPrimeLocationOutcome(GeolocationHeaderPrimeLocationOutcome outcome) {
  base::UmaHistogramEnumeration(kHistogramPrimeLocationOutcome, outcome);
}

void RecordGetLocationOutcome(GeolocationHeaderGetLocationOutcome outcome,
                              bool for_automatic_sending) {
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramGetLocationOutcomePrefix,
                    for_automatic_sending ? kTriggerAutomaticSuffix
                                          : kTriggerInlineSuggestionSuffix}),
      outcome);
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
  if (!base::FeatureList::IsEnabled(omnibox::kPlatformAgnosticXGeo)) {
    return;
  }

  if (geolocation_.is_bound()) {
    RecordPrimeLocationOutcome(
        GeolocationHeaderPrimeLocationOutcome::kNotTriedAlreadyBound);
    return;
  }
  if (!template_url_service_) {
    RecordPrimeLocationOutcome(
        GeolocationHeaderPrimeLocationOutcome::kNotTriedNoDefaultProvider);
    return;
  }

  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider) {
    RecordPrimeLocationOutcome(
        GeolocationHeaderPrimeLocationOutcome::kNotTriedNoDefaultProvider);
    return;
  }

  if (!default_provider->send_x_geo_header()) {
    RecordPrimeLocationOutcome(GeolocationHeaderPrimeLocationOutcome::
                                   kNotTriedProviderDoesNotAcceptHeader);
    return;
  }

  GURL requesting_url = default_provider->GenerateSearchURL(
      template_url_service_->search_terms_data());

  bool is_ills_enabled =
      base::FeatureList::IsEnabled(omnibox::kInlineLocationSignaling);

  if (!requesting_url.is_valid() ||
      !requesting_url.SchemeIs(url::kHttpsScheme)) {
    RecordPrimeLocationOutcome(
        GeolocationHeaderPrimeLocationOutcome::kNotTriedInvalidUrlOrInsecure);
    last_position_.reset();
    return;
  }
  if (!IsAllowedByPermission(requesting_url) && !is_ills_enabled) {
    RecordPrimeLocationOutcome(GeolocationHeaderPrimeLocationOutcome::
                                   kNotTriedPermissionStatusMismatch);
    last_position_.reset();
    return;
  }

  // If the location is fresh and matches targeted precision requirements, there
  // is no need to query for a new one.
  if (HasCachedLocation() &&
      last_position_->is_precise == HasPrecisePermission(requesting_url)) {
    base::TimeDelta age = location_age_for_testing_.value_or(
        base::Time::Now() - last_position_->timestamp);
    if (age <= kMaxLocationAgeForPriming) {
      RecordPrimeLocationOutcome(
          GeolocationHeaderPrimeLocationOutcome::kNotTriedCachedLocationFresh);
      return;
    }
  }

  bool use_cache_only =
      is_ills_enabled && !IsAllowedByPermission(requesting_url);

  if (!EnsureGeolocationServiceConnection(requesting_url, use_cache_only)) {
    RecordPrimeLocationOutcome(
        GeolocationHeaderPrimeLocationOutcome::kTriedFailedConnection);
    return;
  }

  auto callback = base::BindOnce(&GeolocationHeaderService::OnLocationUpdate,
                                 weak_factory_.GetWeakPtr());
  if (use_cache_only) {
    RecordPrimeLocationOutcome(
        GeolocationHeaderPrimeLocationOutcome::kTriedQueryCachedPosition);
    geolocation_->QueryCachedPosition(std::move(callback));
  } else {
    RecordPrimeLocationOutcome(
        GeolocationHeaderPrimeLocationOutcome::kTriedQueryNextPosition);
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

std::optional<GeolocationAccuracy>
GeolocationHeaderService::GetCachedLocationAccuracy() const {
  if (!HasCachedLocation()) {
    return std::nullopt;
  }
  return last_position_->is_precise ? GeolocationAccuracy::kPrecise
                                    : GeolocationAccuracy::kApproximate;
}

std::optional<std::string> GeolocationHeaderService::GetLocationHeader(
    const GURL& url,
    bool for_automatic_sending) {
  if (!base::FeatureList::IsEnabled(omnibox::kPlatformAgnosticXGeo)) {
    return std::nullopt;
  }

  if (!url.SchemeIs(url::kHttpsScheme)) {
    RecordGetLocationOutcome(
        GeolocationHeaderGetLocationOutcome::kInsecureConnection,
        for_automatic_sending);
    return std::nullopt;
  }

  if (!IsUrlEligibleForLocationHeader(url)) {
    RecordGetLocationOutcome(
        GeolocationHeaderGetLocationOutcome::kIneligibleUrl,
        for_automatic_sending);
    return std::nullopt;
  }

  // If this call is for the purpose of sending the header automatically, the
  // DSE should have permission. If this call is for the purpose of building
  // omnibox suggestion, then it should only be allowed if the DSE does NOT have
  // permission.
  if (for_automatic_sending != IsAllowedByPermission(url)) {
    RecordGetLocationOutcome(
        GeolocationHeaderGetLocationOutcome::kPermissionStateMismatch,
        for_automatic_sending);
    return std::nullopt;
  }

  if (!HasCachedLocation()) {
    RecordGetLocationOutcome(
        GeolocationHeaderGetLocationOutcome::kNoCachedLocation,
        for_automatic_sending);
    return std::nullopt;
  }

  // For automatic sending, respect permission granularity.
  // Note: For the interactive flow (for_automatic_sending == false), we bypass
  // this precision check because the user's click on the signaling row
  // constitutes explicit consent to send the cached location as-is, and the UI
  // wording transparently reflects the accuracy being sent ("Use precise
  // location").
  if (for_automatic_sending && last_position_->is_precise &&
      !HasPrecisePermission(url)) {
    RecordGetLocationOutcome(
        GeolocationHeaderGetLocationOutcome::kHeaderGranularityMismatch,
        for_automatic_sending);
    return std::nullopt;
  }

  RecordGetLocationOutcome(GeolocationHeaderGetLocationOutcome::kSuccess,
                           for_automatic_sending);

  return SerializeXGeoHeader(*last_position_);
}

void GeolocationHeaderService::RecordInlineLocationSuggestionShown(
    OmniboxInlineLocationSuggestionShown shown_state,
    size_t match_index) const {
  std::optional<PermissionSetting> permission = GetDSEPermissionSetting();
  if (!permission.has_value()) {
    return;
  }

  const auto& delegate =
      content_settings::PermissionSettingsRegistry::GetInstance()
          ->Get(content_settings::GeolocationContentSettingsType())
          ->delegate();
  bool is_denied = delegate.IsBlocked(*permission);
  bool is_ask = delegate.IsUndecided(*permission);

  if (!is_ask && !is_denied) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({is_denied ? kHistogramInlineLocationSuggestionDenyPrefix
                              : kHistogramInlineLocationSuggestionAskPrefix,
                    kHistogramInlineLocationSuggestionShownSuffix}),
      shown_state);

  if (shown_state ==
      OmniboxInlineLocationSuggestionShown::kLocationSuggestionShown) {
    base::UmaHistogramExactLinear(kHistogramInlineLocationSuggestionIndex,
                                  match_index, 20);
  }
}

bool GeolocationHeaderService::IsUrlEligibleForLocationHeader(
    const GURL& url) const {
  if (!url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  if (!template_url_service_) {
    return false;
  }
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider) {
    return false;
  }

  // Only check send_x_geo_header if the feature is enabled, because this
  // function is also used to check if a URL is eligible for the XGeo header
  // even with the feature off (for legacy behavior metrics).
  if (base::FeatureList::IsEnabled(omnibox::kPlatformAgnosticXGeo) &&
      !default_provider->send_x_geo_header()) {
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

void GeolocationHeaderService::MaybeRecordInlineLocationSuggestionClicked(
    const AutocompleteMatch& match) const {
  if (!match.subtypes.contains(omnibox::SUBTYPE_LOCATION_SUGGEST_TRIGGER)) {
    return;
  }

  std::optional<PermissionSetting> permission = GetDSEPermissionSetting();
  if (!permission.has_value()) {
    return;
  }

  const auto& delegate =
      content_settings::PermissionSettingsRegistry::GetInstance()
          ->Get(content_settings::GeolocationContentSettingsType())
          ->delegate();

  bool is_denied = delegate.IsBlocked(*permission);
  bool is_ask = delegate.IsUndecided(*permission);

  if (!is_ask && !is_denied) {
    return;
  }

  bool parent_clicked = !match.extra_headers.contains(kXGeoHeader);

  // Build the histogram name with the right prefix and the right suffix.
  base::UmaHistogramBoolean(
      base::StrCat({is_denied ? kHistogramInlineLocationSuggestionDenyPrefix
                              : kHistogramInlineLocationSuggestionAskPrefix,
                    parent_clicked
                        ? kHistogramInlineLocationSuggestionParentClickedSuffix
                        : kHistogramInlineLocationSuggestionClickedSuffix}),
      true);
}

bool GeolocationHeaderService::IsAllowedByPermission(const GURL& url) const {
  if (!HasDeviceLocationPermission(GeolocationAccuracy::kApproximate)) {
    return false;
  }

  return content_settings::PermissionSettingsRegistry::GetInstance()
      ->Get(content_settings::GeolocationContentSettingsType())
      ->delegate()
      .IsAnyPermissionAllowed(GetPermissionSetting(url));
}

bool GeolocationHeaderService::HasPrecisePermission(const GURL& url) const {
  if (!HasDeviceLocationPermission(GeolocationAccuracy::kPrecise)) {
    return false;
  }

  return std::visit(absl::Overload(
                        [](const GeolocationSetting& geo_setting) {
                          return geo_setting.precise ==
                                 PermissionOption::kAllowed;
                        },
                        [](ContentSetting content_setting) {
                          return content_setting == CONTENT_SETTING_ALLOW;
                        }),
                    GetPermissionSetting(url));
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
      geolocation_.BindNewPipeAndPassReceiver(),
      url::Origin::Create(requesting_url),
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

std::optional<PermissionSetting>
GeolocationHeaderService::GetDSEPermissionSetting() const {
  if (!template_url_service_) {
    return std::nullopt;
  }
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider || !default_provider->send_x_geo_header()) {
    return std::nullopt;
  }

  GURL url = default_provider->GenerateSearchURL(
      template_url_service_->search_terms_data());

  return GetPermissionSetting(url);
}

PermissionSetting GeolocationHeaderService::GetPermissionSetting(
    const GURL& url) const {
  return settings_map_->GetPermissionSetting(
      url, url, content_settings::GeolocationContentSettingsType());
}
