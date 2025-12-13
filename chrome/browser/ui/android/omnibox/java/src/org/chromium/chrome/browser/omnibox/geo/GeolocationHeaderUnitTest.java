// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.location.Location;
import android.os.SystemClock;

import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.Granularity;
import com.google.android.gms.location.LocationListener;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationServices;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/** Robolectric tests for {@link GeolocationHeader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
@EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
public class GeolocationHeaderUnitTest {
    private static final String SEARCH_URL = "https://www.google.com/search?q=potatoes";

    private static final double LOCATION_LAT = 20.3;
    private static final double LOCATION_LONG = 155.8;
    private static final float LOCATION_ACCURACY = 20f;
    private static final long LOCATION_TIME = 400;
    // Encoded location for LOCATION_LAT, LOCATION_LONG, LOCATION_ACCURACY and LOCATION_TIME.
    private static final String ENCODED_PROTO_LOCATION = "CAEQDBiAtRgqCg3AiBkMFYAx3Vw9AECcRg==";
    private int mRefreshLastKnownLocationCount;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock Profile mProfileMock;
    @Mock WebContents mWebContentsMock;
    @Mock TemplateUrlService mTemplateUrlServiceMock;
    @Mock FusedLocationProviderClient mLocationProviderClient;
    @Captor private ArgumentCaptor<LocationListener> mLocationListenerCaptor;
    @Captor private ArgumentCaptor<LocationRequest> mLocationRequestCaptor;

    @Before
    public void setUp() {
        UrlUtilitiesJni.setInstanceForTesting(mUrlUtilitiesJniMock);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeJniMock);
        GeolocationTracker.setLocationForTesting(null, null);
        GeolocationTracker.setLocationAgeForTesting(null);
        GeolocationHeader.setAppPermissionsForTesting(/* hasCoarse= */ true, /* hasFine= */ true);
        // This is to reset `sCurrentLocationRequested`.
        GeolocationHeader.stopListeningForLocationUpdates();
        when(mWebsitePreferenceBridgeJniMock.getPermissionSettingForOrigin(
                        any(BrowserContextHandle.class), eq(ContentSettingsType.GEOLOCATION),
                        anyString(), anyString()))
                .thenReturn(ContentSetting.ALLOW);
        setSiteGeolocationPermissions(
                /* approximate= */ ContentSetting.ALLOW, /* precise= */ ContentSetting.ALLOW);
        when(mWebsitePreferenceBridgeJniMock.isDSEOrigin(
                        any(BrowserContextHandle.class), anyString()))
                .thenReturn(true);
        when(mUrlUtilitiesJniMock.isGoogleSearchUrl(anyString())).thenReturn(true);
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mTemplateUrlServiceMock.getUrlForSearchQuery(anyString()))
                .thenReturn("https://example.com/");
        when(mTemplateUrlServiceMock.isDefaultSearchEngineGoogle()).thenReturn(true);
        mRefreshLastKnownLocationCount = 0;
        GeolocationTracker.setRefreshLastKnownLocationRunnableForTesting(
                () -> mRefreshLastKnownLocationCount++);
        ShadowLocationServices.sFusedLocationProviderClient = mLocationProviderClient;
    }

    @Test
    public void testEncodeProtoLocation() {
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        String encodedProtoLocation = GeolocationHeader.encodeProtoLocation(location);
        assertEquals(ENCODED_PROTO_LOCATION, encodedProtoLocation);
    }

    @Test
    public void testGetGeoHeaderFreshLocation() {
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        GeolocationTracker.setLocationForTesting(location, null);
        // 1 minute should be good enough and not require visible networks.
        GeolocationTracker.setLocationAgeForTesting(1 * 60 * 1000L);
        String header =
                GeolocationHeader.getGeoHeader(SEARCH_URL, mProfileMock, mTemplateUrlServiceMock);
        assertEquals("X-Geo: w " + ENCODED_PROTO_LOCATION, header);
    }

    @Test
    public void testGetGeoHeaderOld() {
        checkOldLocation("X-Geo: w " + ENCODED_PROTO_LOCATION);
    }

    @Test
    public void testGetGeoHeaderOldLocationAppPermissionDenied() {
        GeolocationHeader.setAppPermissionsForTesting(/* hasCoarse= */ false, /* hasFine= */ false);
        // Nothing should be included when app permission is missing.
        checkOldLocation(null);
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.USE_FUSED_LOCATION_PROVIDER)
    public void testPrimeLocationForGeoHeader() {
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(1, mRefreshLastKnownLocationCount);
    }

    @Test
    public void testPrimeLocationForGeoHeaderPermissionOff() {
        GeolocationHeader.setAppPermissionsForTesting(/* hasCoarse= */ false, /* hasFine= */ false);
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(0, mRefreshLastKnownLocationCount);
    }

    @Test
    public void testPrimeLocationForGeoHeaderDseAutograntOff() {
        when(mWebsitePreferenceBridgeJniMock.getPermissionSettingForOrigin(
                        any(BrowserContextHandle.class), eq(ContentSettingsType.GEOLOCATION),
                        anyString(), anyString()))
                .thenReturn(ContentSetting.ASK);
        setSiteGeolocationPermissions(
                /* approximate= */ ContentSetting.ASK, /* precise= */ ContentSetting.ASK);
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(0, mRefreshLastKnownLocationCount);
    }

    @Test
    @Config(shadows = {ShadowLocationServices.class})
    public void testFusedLocationProvider() {
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        verify(mLocationProviderClient)
                .requestLocationUpdates(
                        mLocationRequestCaptor.capture(),
                        mLocationListenerCaptor.capture(),
                        eq(null));

        LocationRequest actualRequest = mLocationRequestCaptor.getValue();
        assertEquals(
                OmniboxFeatures.sGeolocationRequestMaxLocationAge.getValue(),
                actualRequest.getMaxUpdateAgeMillis());
        assertEquals(
                OmniboxFeatures.sGeolocationRequestUpdateInterval.getValue(),
                actualRequest.getMinUpdateIntervalMillis());
        assertEquals(
                OmniboxFeatures.sGeolocationRequestPriority.getValue(),
                actualRequest.getPriority());
        assertEquals(Granularity.GRANULARITY_FINE, actualRequest.getGranularity());

        Location mockLocation = generateMockLocation("network", LOCATION_TIME);
        mLocationListenerCaptor.getValue().onLocationChanged(mockLocation);
        assertEquals(mockLocation, GeolocationHeader.getLastKnownLocation(/* useFine= */ true));
        assertEquals(0, mRefreshLastKnownLocationCount);

        GeolocationHeader.stopListeningForLocationUpdates();
        verify(mLocationProviderClient).removeLocationUpdates(mLocationListenerCaptor.getValue());

        doThrow(new RuntimeException())
                .when(mLocationProviderClient)
                .requestLocationUpdates(
                        any(LocationRequest.class), any(LocationListener.class), eq(null));
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);

        assertEquals(1, mRefreshLastKnownLocationCount);
    }

    @Test
    @Config(shadows = {ShadowLocationServices.class})
    public void testFusedLocationProvider_SitePrecisePermissionGranted() {
        // App-level and site-level permissions are granted at precise-level by default in `setUp`.

        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        verify(mLocationProviderClient)
                .requestLocationUpdates(
                        mLocationRequestCaptor.capture(),
                        mLocationListenerCaptor.capture(),
                        eq(null));

        LocationRequest actualRequest = mLocationRequestCaptor.getValue();
        assertEquals(Granularity.GRANULARITY_FINE, actualRequest.getGranularity());

        Location mockLocation = generateMockLocation("network", LOCATION_TIME);
        mLocationListenerCaptor.getValue().onLocationChanged(mockLocation);
        assertEquals(mockLocation, GeolocationHeader.getLastKnownLocation(/* useFine= */ true));
        assertEquals(0, mRefreshLastKnownLocationCount);
    }

    @Test
    @Config(shadows = {ShadowLocationServices.class})
    public void testFusedLocationProvider_SiteApproximatePermissionGranted() {
        // App-level permissions are granted by default in setUp.
        // Site-level permission is granted for approximate.
        setSiteGeolocationPermissions(
                /* approximate= */ ContentSetting.ALLOW, /* precise= */ ContentSetting.BLOCK);

        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        verify(mLocationProviderClient)
                .requestLocationUpdates(
                        mLocationRequestCaptor.capture(),
                        mLocationListenerCaptor.capture(),
                        eq(null));

        LocationRequest actualRequest = mLocationRequestCaptor.getValue();
        assertEquals(Granularity.GRANULARITY_COARSE, actualRequest.getGranularity());

        Location mockLocation = generateMockLocation("network", LOCATION_TIME);
        mLocationListenerCaptor.getValue().onLocationChanged(mockLocation);
        assertEquals(mockLocation, GeolocationHeader.getLastKnownLocation(/* useFine= */ false));
        assertEquals(0, mRefreshLastKnownLocationCount);
    }

    @Test
    @Config(shadows = {ShadowLocationServices.class})
    public void testFusedLocationProvider_SitePermissionDenied() {
        // App-level permissions are granted by default in setUp.
        // Site-level permission is denied.
        setSiteGeolocationPermissions(
                /* approximate= */ ContentSetting.BLOCK, /* precise= */ ContentSetting.BLOCK);

        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        verify(mLocationProviderClient, never())
                .requestLocationUpdates(
                        any(LocationRequest.class), any(LocationListener.class), eq(null));
        assertEquals(0, mRefreshLastKnownLocationCount);
    }

    @Test
    @Config(shadows = {ShadowLocationServices.class})
    public void testFusedLocationProvider_AppCoarseSitePrecisePermission() {
        // App has only coarse permission.
        GeolocationHeader.setAppPermissionsForTesting(/* hasCoarse= */ true, /* hasFine= */ false);

        // Site has precise permission.
        setSiteGeolocationPermissions(
                /* approximate= */ ContentSetting.ALLOW, /* precise= */ ContentSetting.ALLOW);

        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        verify(mLocationProviderClient)
                .requestLocationUpdates(
                        mLocationRequestCaptor.capture(),
                        mLocationListenerCaptor.capture(),
                        eq(null));

        // The request should be coarse because the app permission is the most restrictive.
        LocationRequest actualRequest = mLocationRequestCaptor.getValue();
        assertEquals(Granularity.GRANULARITY_COARSE, actualRequest.getGranularity());

        // Verify that the location is treated as not fine.
        Location mockLocation = generateMockLocation("network", LOCATION_TIME);
        mLocationListenerCaptor.getValue().onLocationChanged(mockLocation);
        assertEquals(mockLocation, GeolocationHeader.getLastKnownLocation(/* useFine= */ false));
        assertEquals(0, mRefreshLastKnownLocationCount);
    }

    private void setSiteGeolocationPermissions(
            @ContentSetting int approximate, @ContentSetting int precise) {
        when(mWebsitePreferenceBridgeJniMock.getGeolocationSettingForOrigin(
                        any(BrowserContextHandle.class),
                        eq(ContentSettingsType.GEOLOCATION_WITH_OPTIONS),
                        anyString(),
                        anyString()))
                .thenReturn(new GeolocationSetting(approximate, precise));
    }

    private void checkOldLocation(String expectedHeader) {
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        GeolocationTracker.setLocationForTesting(location, null);
        // 6 minutes should hit the age limit, but the feature is off.
        GeolocationTracker.setLocationAgeForTesting(6 * 60 * 1000L);
        String header =
                GeolocationHeader.getGeoHeader(SEARCH_URL, mProfileMock, mTemplateUrlServiceMock);
        assertEquals(expectedHeader, header);
    }

    private Location generateMockLocation(String provider, long time) {
        Location location = new Location(provider);
        location.setLatitude(LOCATION_LAT);
        location.setLongitude(LOCATION_LONG);
        location.setAccuracy(LOCATION_ACCURACY);
        location.setTime(time);
        location.setElapsedRealtimeNanos(
                SystemClock.elapsedRealtimeNanos() + 1000000 * (time - System.currentTimeMillis()));
        return location;
    }

    /** Shadow for LocationServices */
    @Implements(LocationServices.class)
    public static class ShadowLocationServices {
        static FusedLocationProviderClient sFusedLocationProviderClient;

        @Implementation
        public static FusedLocationProviderClient getFusedLocationProviderClient(Context context) {
            return sFusedLocationProviderClient;
        }
    }
}
