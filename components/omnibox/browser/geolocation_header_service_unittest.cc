// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/geolocation_header_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/location/android/mock_location_settings.h"
#endif
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Optional;
using testing::StartsWith;

namespace {
// Encoded location for:
// lat: 20.3, long: 155.8, accuracy: 20.0, time: 123456789ms since epoch.
// Textproto contents for precise location:
// {
//  role: CURRENT_LOCATION
//  producer: DEVICE_LOCATION
//  timestamp: 123456789000
//  latlng: {
//    latitude_e7 : 203000000
//    longitude_e7: 1558000000
//  }
//  radius: 20000.0
//  permission_granularity: PERMISSION_GRANULARITY_FINE
// }
constexpr double kTestLat = 20.3;
constexpr double kTestLong = 155.8;
constexpr double kTestAccuracy = 20.0;
constexpr char kGoogleUrl[] = "https://www.google.com/search?q=test";
constexpr char kLocationProtoPrefix[] = "w ";

}  // namespace

class GeolocationHeaderServiceTest : public testing::Test {
 public:
  GeolocationHeaderServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        {omnibox::kPlatformAgnosticXGeo,
         omnibox::kOmniboxXGeoPermissionGranularity,
         content_settings::features::kApproximateGeolocationPermission},
        {});
  }

  void SetUp() override {
    search_engines_test_environment_ =
        std::make_unique<search_engines::SearchEnginesTestEnvironment>();

    HostContentSettingsMap::RegisterProfilePrefs(
        search_engines_test_environment_->pref_service().registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &search_engines_test_environment_->pref_service(), false, true, false,
        false);
    SetAppLevelPermission(/*granted=*/true);
    UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                   /*is_precise=*/true);
  }

  void TearDown() override {
    settings_map_->ShutdownOnUIThread();
    search_engines_test_environment_ = nullptr;
  }

  std::unique_ptr<GeolocationHeaderService> CreateService() {
#if BUILDFLAG(IS_ANDROID)
    auto mock = std::make_unique<MockLocationSettings>();
    MockLocationSettings::SetLocationStatus(
        has_android_coarse_, has_android_fine_,
        /*is_system_location_setting_enabled=*/true);
    return std::make_unique<GeolocationHeaderService>(
        settings_map(), template_url_service(), std::move(mock));
#else
    return std::make_unique<GeolocationHeaderService>(settings_map(),
                                                      template_url_service());
#endif
  }

  void SetAppLevelPermission(bool granted, bool fine_granted = true) {
    has_android_coarse_ = granted;
    has_android_fine_ = granted && fine_granted;
    permissions_client_.SetHasDevicePermission(granted);
  }

  void SetSitePermissionWithOptions(const GURL& url,
                                    GeolocationSetting setting) {
    settings_map_->SetPermissionSettingDefaultScope(
        url, url, content_settings::GeolocationContentSettingsType(), setting);
  }

  void SetDefaultSearchProviderUrl(std::string_view url) {
    TemplateURLData data;
    // Append {searchTerms} if not present so IsSearchURL() returns true.
    std::string template_url(url);
    if (template_url.find("{searchTerms}") == std::string::npos) {
      if (template_url.back() != '/') {
        template_url += "/";
      }
      template_url += "search?q={searchTerms}";
    }
    data.SetURL(template_url);
    if (url.find("google.com") != std::string::npos) {
      data.SetKeyword(u"google.com");
    }
    TemplateURL* t_url =
        template_url_service()->Add(std::make_unique<TemplateURL>(data));
    template_url_service()->SetUserSelectedDefaultSearchProvider(t_url);
  }

  void SetupGoogleDseWithPermissions() {
    SetDefaultSearchProviderUrl(kGoogleUrl);
    SetSitePermissionWithOptions(
        GURL(kGoogleUrl),
        {PermissionOption::kAllowed, PermissionOption::kAllowed});
  }

  void UpdateLocation(double latitude,
                      double longitude,
                      double accuracy,
                      base::Time timestamp,
                      bool is_precise) {
    auto result = device::mojom::GeopositionResult::NewPosition(
        device::mojom::Geoposition::New());
    result->get_position()->latitude = latitude;
    result->get_position()->longitude = longitude;
    result->get_position()->accuracy = accuracy;
    result->get_position()->timestamp = timestamp;
    result->get_position()->is_precise = is_precise;
    geolocation_overrider_.UpdateLocation(std::move(result));
  }

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }
  TemplateURLService* template_url_service() {
    return search_engines_test_environment_->template_url_service();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<HostContentSettingsMap> settings_map_;
  permissions::TestPermissionsClient permissions_client_;
  bool has_android_coarse_ = true;
  bool has_android_fine_ = true;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<search_engines::SearchEnginesTestEnvironment>
      search_engines_test_environment_;

  device::ScopedGeolocationOverrider geolocation_overrider_{0.0, 0.0};
};

TEST_F(GeolocationHeaderServiceTest, PermissionChecks) {
  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  GURL url(kGoogleUrl);
  SetupGoogleDseWithPermissions();

  // 1. Default is ASK, so not allowed. PrimeLocation shouldn't fetch.
  service->PrimeLocation();
  EXPECT_FALSE(service->HasCachedLocation());

  // 2. Set site permission to ALLOW. Allowed since app-level is granted.
  SetSitePermissionWithOptions(
      url, {PermissionOption::kAllowed, PermissionOption::kAllowed});
  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  // 3. Revoke app-level permission.
  SetAppLevelPermission(/*granted=*/false);
  std::unique_ptr<GeolocationHeaderService> service2 = CreateService();
  service2->PrimeLocation();
  EXPECT_FALSE(service2->HasCachedLocation());

  // 4. Restore app-level, revoke site-level.
  SetAppLevelPermission(/*granted=*/true);
  std::unique_ptr<GeolocationHeaderService> service3 = CreateService();
  SetSitePermissionWithOptions(
      url, {PermissionOption::kDenied, PermissionOption::kDenied});
  service3->PrimeLocation();
  EXPECT_FALSE(service3->HasCachedLocation());
}

// Verifies that the service only requests a single position update, destroying
// the location watcher immediately after receiving the position.
TEST_F(GeolocationHeaderServiceTest, SingleQueryInstance) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  SetupGoogleDseWithPermissions();

  EXPECT_EQ(0u, geolocation_overrider_.GetGeolocationInstanceCount());

  // Pause the overrider so the callback is not immediately fired.
  geolocation_overrider_.Pause();

  service->PrimeLocation();

  // Wait until the Geolocation instance is created in the device service, this
  // is not flaky because the overrider is paused.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_overrider_.GetGeolocationInstanceCount() == 1u;
  }));

  // Resume the overrider. This fulfills the QueryNextPosition request, which
  // causes the service to save the location and reset the remote, destroying
  // the instance.
  geolocation_overrider_.Resume();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_overrider_.GetGeolocationInstanceCount() == 0u;
  }));

  EXPECT_TRUE(service->HasCachedLocation());
}

// Verifies that priming requests a location, and that subsequent requests yield
// a valid header.
TEST_F(GeolocationHeaderServiceTest, PrimeAndGetLocation) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  GURL url(kGoogleUrl);
  SetupGoogleDseWithPermissions();

  EXPECT_FALSE(service->HasCachedLocation());

  service->PrimeLocation();

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  std::optional<std::string> header = service->GetLocationHeader(url);
  EXPECT_THAT(header, Optional(StartsWith(kLocationProtoPrefix)));
}

// Verifies that approximate site permissions still allow for the generation of
// a location header.
TEST_F(GeolocationHeaderServiceTest, ApproximatePermission) {
  base::Time timestamp = base::Time::UnixEpoch() + base::Milliseconds(400);
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, timestamp,
                 /*is_precise=*/false);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  service->SetLocationAgeForTesting(base::Minutes(1));
  GURL url(kGoogleUrl);
  SetDefaultSearchProviderUrl(url.spec());

  // Grant only approximate permission.
  SetSitePermissionWithOptions(
      url, {PermissionOption::kAllowed, PermissionOption::kDenied});

  service->PrimeLocation();

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  std::optional<std::string> header = service->GetLocationHeader(url);
  ASSERT_TRUE(header.has_value());
  // The expected string for COARSE location matching the Java tests.
  EXPECT_EQ(*header, "w CAEQDBiAtRgqCg3AiBkMFYAx3Vw9AECcRsgBAQ==");
}

// Verifies that location headers are strictly blocked for non-HTTPS URL
// requests.
TEST_F(GeolocationHeaderServiceTest, NonHttpsUrl) {
  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  GURL url("http://www.google.com");
  SetDefaultSearchProviderUrl(url.spec());
  SetSitePermissionWithOptions(
      url, {PermissionOption::kAllowed, PermissionOption::kAllowed});

  service->PrimeLocation();
  EXPECT_FALSE(service->HasCachedLocation());
}

// Verifies the structural integrity and base64url encoding of the X-Geo header
// payload.
TEST_F(GeolocationHeaderServiceTest, Serialization) {
  base::Time timestamp = base::Time::UnixEpoch() + base::Milliseconds(400);
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, timestamp,
                 /*is_precise=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  service->SetLocationAgeForTesting(base::Minutes(1));
  GURL url(kGoogleUrl);
  SetupGoogleDseWithPermissions();

  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  std::optional<std::string> header = service->GetLocationHeader(url);
  ASSERT_TRUE(header.has_value());
  // The expected string corresponds to the exact parameters above matching the
  // Java tests.
  EXPECT_EQ(*header, "w CAEQDBiAtRgqCg3AiBkMFYAx3Vw9AECcRsgBAg==");
}

// Verifies that cached location data correctly expires after 24 hours,
// preventing stale location leakage.
TEST_F(GeolocationHeaderServiceTest, OldLocation) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  GURL url(kGoogleUrl);
  SetupGoogleDseWithPermissions();

  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  // Advance time by 23 hours. Location should still be valid.
  task_environment_.FastForwardBy(base::Hours(23));
  EXPECT_TRUE(service->HasCachedLocation());
  EXPECT_TRUE(service->GetLocationHeader(url).has_value());

  // Advance time by 2 more hours (25 total). Location should be expired.
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_FALSE(service->HasCachedLocation());
  EXPECT_FALSE(service->GetLocationHeader(url).has_value());
}

// Verifies that location headers are not appended to arbitrary non-Default
// Search Engine URLs.
TEST_F(GeolocationHeaderServiceTest, NonDseUrl) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();
  GURL dse_url("https://www.bing.com");
  SetDefaultSearchProviderUrl(dse_url.spec());
  SetSitePermissionWithOptions(
      dse_url, {PermissionOption::kAllowed, PermissionOption::kAllowed});

  // Prime location with the DSE URL
  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  // Try to get header for a completely different URL
  GURL non_dse_url("https://www.yahoo.com");
  SetSitePermissionWithOptions(
      non_dse_url, {PermissionOption::kAllowed, PermissionOption::kAllowed});
  std::optional<std::string> header = service->GetLocationHeader(non_dse_url);
  EXPECT_FALSE(header.has_value());
}

// Verifies that Google domains are NOT eligible for the header if their origin
// differs from the DSE origin, preventing cross-TLD leakage.
TEST_F(GeolocationHeaderServiceTest, GoogleFallbackUrl) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  // Use a static search URL without {searchTerms} so GenerateSearchURL returns
  // it identically.
  GURL dse_url("https://www.google.com/search?q=test");
  SetDefaultSearchProviderUrl(dse_url.spec());

  SetSitePermissionWithOptions(
      dse_url, {PermissionOption::kAllowed, PermissionOption::kAllowed});

  // Use a Google search URL that is not exactly the DSE origin but is Google
  GURL google_fallback_url("https://www.google.co.uk/search?q=test");
  SetSitePermissionWithOptions(
      google_fallback_url,
      {PermissionOption::kAllowed, PermissionOption::kAllowed});

  // Prime location with the DSE URL
  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  std::optional<std::string> header =
      service->GetLocationHeader(google_fallback_url);
  EXPECT_FALSE(header.has_value());
}

// Verifies behavior when the OS grants only coarse location, but the site has
// precise location allowed.
TEST_F(GeolocationHeaderServiceTest, AppCoarseSitePrecisePermission) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/false);

  GURL url(kGoogleUrl);
  SetupGoogleDseWithPermissions();

  SetAppLevelPermission(/*granted=*/true, /*fine_granted=*/false);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  std::optional<std::string> header = service->GetLocationHeader(url);
  EXPECT_THAT(header, Optional(StartsWith(kLocationProtoPrefix)));
}

// Verifies that a precise location from the device is appropriately handled
// when the site only grants coarse permissions.
TEST_F(GeolocationHeaderServiceTest, PositionPreciseSiteCoarsePermission) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  GURL url(kGoogleUrl);
  SetDefaultSearchProviderUrl(url.spec());
  SetSitePermissionWithOptions(
      url, {PermissionOption::kAllowed, PermissionOption::kDenied});

  SetAppLevelPermission(/*granted=*/true, /*fine_granted=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  std::optional<std::string> header = service->GetLocationHeader(url);
  EXPECT_FALSE(header.has_value());
}

// Verifies that a cached precise location is not sent if precise permission is
// revoked before GetLocationHeader.
TEST_F(GeolocationHeaderServiceTest, PreciseToCoarseDowngradeLeak) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  GURL url(kGoogleUrl);
  SetupGoogleDseWithPermissions();
  SetAppLevelPermission(/*granted=*/true, /*fine_granted=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  // Downgrade permission to coarse.
  SetSitePermissionWithOptions(
      url, {PermissionOption::kAllowed, PermissionOption::kDenied});

  // The cached location should be dropped and nullopt returned.
  std::optional<std::string> header = service->GetLocationHeader(url);
  EXPECT_FALSE(header.has_value());
}

// Verifies that only a search URL receives location.
TEST_F(GeolocationHeaderServiceTest, ConsistentHeader) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  GURL dse_url(kGoogleUrl);
  SetDefaultSearchProviderUrl(dse_url.spec());
  SetSitePermissionWithOptions(
      dse_url, {PermissionOption::kAllowed, PermissionOption::kAllowed});

  service->PrimeLocation();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  // X-Geo should be sent for Google search results page URLs.
  std::optional<std::string> header = service->GetLocationHeader(dse_url);
  EXPECT_THAT(header, Optional(StartsWith(kLocationProtoPrefix)));

  // Legacy Google search URLs (e.g. /webhp) should also be supported via the
  // fallback if the origin matches the DSE.
  GURL legacy_search_url("https://www.google.com/webhp?#q=dinosaurs");
  SetSitePermissionWithOptions(legacy_search_url, {PermissionOption::kAllowed,
                                                   PermissionOption::kAllowed});
  std::optional<std::string> legacy_header =
      service->GetLocationHeader(legacy_search_url);
  EXPECT_THAT(legacy_header, Optional(StartsWith(kLocationProtoPrefix)));

  // But only the current CCTLD.
  GURL diff_cctld("https://www.google.co.jp/webhp?#q=dinosaurs");
  SetSitePermissionWithOptions(
      diff_cctld, {PermissionOption::kAllowed, PermissionOption::kAllowed});
  EXPECT_FALSE(service->GetLocationHeader(diff_cctld).has_value());

  // X-Geo shouldn't be sent with URLs that aren't the Google search results
  // page.
  GURL invalid_url("invalid$url");
  EXPECT_FALSE(service->GetLocationHeader(invalid_url).has_value());

  GURL chrome_fr("https://www.chrome.fr/");
  SetSitePermissionWithOptions(
      chrome_fr, {PermissionOption::kAllowed, PermissionOption::kAllowed});
  EXPECT_FALSE(service->GetLocationHeader(chrome_fr).has_value());

  GURL google_homepage("https://www.google.com/");
  EXPECT_FALSE(service->GetLocationHeader(google_homepage).has_value());

  GURL google_maps("https://www.google.com/maps");
  EXPECT_FALSE(service->GetLocationHeader(google_maps).has_value());

  // X-Geo shouldn't be sent over HTTP.
  GURL http_search("http://www.google.com/search?q=potatoes");
  SetSitePermissionWithOptions(
      http_search, {PermissionOption::kAllowed, PermissionOption::kAllowed});
  EXPECT_FALSE(service->GetLocationHeader(http_search).has_value());

  GURL http_webhp("http://www.google.com/webhp?#q=dinosaurs");
  SetSitePermissionWithOptions(
      http_webhp, {PermissionOption::kAllowed, PermissionOption::kAllowed});
  EXPECT_FALSE(service->GetLocationHeader(http_webhp).has_value());
}

// Verifies that a fresh cached location prevents querying for a new one.
TEST_F(GeolocationHeaderServiceTest, FreshLocationPreventsQuery) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  auto service = CreateService();
  SetupGoogleDseWithPermissions();

  // 1. First call should fetch.
  geolocation_overrider_.Pause();
  service->PrimeLocation();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_overrider_.GetGeolocationInstanceCount() == 1u;
  }));
  geolocation_overrider_.Resume();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  // 2. Second call should return early, not fetching a new location.
  service->PrimeLocation();
  EXPECT_EQ(0u, geolocation_overrider_.GetGeolocationInstanceCount());
}

// Verifies that the service requests high accuracy from the device service when
// precise permissions are granted.
TEST_F(GeolocationHeaderServiceTest, HighAccuracyHint) {
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  auto service = CreateService();
  GURL url(kGoogleUrl);
  SetDefaultSearchProviderUrl(url.spec());

  // 1. Grant precise permission.
  SetSitePermissionWithOptions(
      url, {PermissionOption::kAllowed, PermissionOption::kAllowed});
  SetAppLevelPermission(/*granted=*/true, /*fine_granted=*/true);

  // Prime location and wait for connection.
  geolocation_overrider_.Pause();
  service->PrimeLocation();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_overrider_.GetGeolocationInstanceCount() == 1u;
  }));
  geolocation_overrider_.Resume();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  // 2. Revoke precise permission (downgrade to approximate).
  SetSitePermissionWithOptions(
      url, {PermissionOption::kAllowed, PermissionOption::kDenied});

  // GetLocationHeader should reset the connection.
  EXPECT_FALSE(service->GetLocationHeader(url).has_value());

  // Prime again. This should create a new connection.
  geolocation_overrider_.Pause();
  service->PrimeLocation();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_overrider_.GetGeolocationInstanceCount() == 1u;
  }));
  geolocation_overrider_.Resume();

  // Wait for the new connection to be established.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));
}

class GeolocationHeaderServiceInlineLocationTest
    : public GeolocationHeaderServiceTest {
 public:
  GeolocationHeaderServiceInlineLocationTest() {
    feature_list_.InitWithFeatures(
        {omnibox::kPlatformAgnosticXGeo,
         omnibox::kOmniboxXGeoPermissionGranularity,
         omnibox::kInlineLocationSignaling,
         content_settings::features::kApproximateGeolocationPermission},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that when the InlineLocationSignaling flag is enabled and the DSE
// lacks permission, PrimeLocation uses the cache-only flow and does not
// trigger active scans.
TEST_F(GeolocationHeaderServiceInlineLocationTest, PrimeLocationCacheOnly) {
  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  // Setup DSE but without permissions (ASK)
  SetDefaultSearchProviderUrl(kGoogleUrl);
  SetSitePermissionWithOptions(
      GURL(kGoogleUrl), {PermissionOption::kAsk, PermissionOption::kAsk});

  // Clear the mock service location by overriding with an error
  geolocation_overrider_.OverrideGeolocation(
      device::mojom::GeopositionResult::NewError(
          device::mojom::GeopositionError::New(
              device::mojom::GeopositionErrorCode::kPositionUnavailable, "",
              "")));

  service->PrimeLocation();

  // Wait for the call to complete (connection reset). If a non-cached location
  // was used, it would stay bound waiting for an update.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !service->is_geolocation_bound_for_testing(); }));

  EXPECT_EQ(geolocation_overrider_.GetQueryCachedPositionCount(), 1u);
  EXPECT_EQ(geolocation_overrider_.GetQueryNextPositionCount(), 0u);
  EXPECT_FALSE(service->HasCachedLocation());
}

// Tests that when the InlineLocationSignaling flag is enabled and the DSE
// has permission, PrimeLocation uses the standard flow and waits for active
// scans.
TEST_F(GeolocationHeaderServiceInlineLocationTest, PrimeLocationStandard) {
  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  SetupGoogleDseWithPermissions();

  // Clear the mock service location by overriding with an error
  geolocation_overrider_.OverrideGeolocation(
      device::mojom::GeopositionResult::NewError(
          device::mojom::GeopositionError::New(
              device::mojom::GeopositionErrorCode::kPositionUnavailable, "",
              "")));

  service->PrimeLocation();

  // Verify it is waiting for update (connection remains bound)
  EXPECT_TRUE(service->is_geolocation_bound_for_testing());

  // Now simulate a fresh update.
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  // Wait for the call to complete (connection reset)
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !service->is_geolocation_bound_for_testing(); }));

  EXPECT_EQ(geolocation_overrider_.GetQueryCachedPositionCount(), 0u);
  EXPECT_EQ(geolocation_overrider_.GetQueryNextPositionCount(), 1u);
  EXPECT_TRUE(service->HasCachedLocation());
}

// Tests that when the InlineLocationSignaling flag is disabled and the DSE
// lacks permission, PrimeLocation aborts early and does not query the service.
TEST_F(GeolocationHeaderServiceTest, PrimeLocationFlagDisabled) {
  std::unique_ptr<GeolocationHeaderService> service = CreateService();

  // Setup DSE but without permissions (ASK)
  SetDefaultSearchProviderUrl(kGoogleUrl);
  SetSitePermissionWithOptions(
      GURL(kGoogleUrl), {PermissionOption::kAsk, PermissionOption::kAsk});

  // Clear the mock service location by overriding with an error
  geolocation_overrider_.OverrideGeolocation(
      device::mojom::GeopositionResult::NewError(
          device::mojom::GeopositionError::New(
              device::mojom::GeopositionErrorCode::kPositionUnavailable, "",
              "")));

  service->PrimeLocation();

  // Since the flag was disabled and DSE not allowed, it should have aborted
  // early.
  EXPECT_EQ(geolocation_overrider_.GetQueryCachedPositionCount(), 0u);
  EXPECT_EQ(geolocation_overrider_.GetQueryNextPositionCount(), 0u);
  EXPECT_FALSE(service->HasCachedLocation());
  EXPECT_FALSE(service->is_geolocation_bound_for_testing());

  // Now grant permission to the DSE and verify that PrimeLocation proceeds.
  SetupGoogleDseWithPermissions();

  service->PrimeLocation();

  // Verify it is waiting for update (connection remains bound)
  EXPECT_TRUE(service->is_geolocation_bound_for_testing());

  // Now simulate a fresh update.
  UpdateLocation(kTestLat, kTestLong, kTestAccuracy, base::Time::Now(),
                 /*is_precise=*/true);

  // Wait for the call to complete (connection reset)
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !service->is_geolocation_bound_for_testing(); }));

  EXPECT_EQ(geolocation_overrider_.GetQueryNextPositionCount(), 1u);
  EXPECT_TRUE(service->HasCachedLocation());
}
