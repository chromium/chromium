// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/** Robolectric tests for {@link GeolocationHeader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class GeolocationHeaderUnitTest {
    private static final String SEARCH_URL = "https://www.google.com/search?q=potatoes";

    private static final double LOCATION_LAT = 20.3;
    private static final double LOCATION_LONG = 155.8;
    private static final float LOCATION_ACCURACY = 20f;
    private static final long LOCATION_TIME = 400;
    // Encoded location for LOCATION_LAT, LOCATION_LONG, LOCATION_ACCURACY and LOCATION_TIME.
    private static final String ENCODED_PROTO_LOCATION = "CAEQDBiAtRgqCg3AiBkMFYAx3Vw9AECcRg==";
    private static int sRefreshLastKnownLocation;

    public @Rule JniMocker mocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock Profile mProfileMock;
    @Mock private Tab mTab;
    @Mock WebContents mWebContentsMock;
    @Mock TemplateUrlService mTemplateUrlServiceMock;
    @Mock FusedLocationProviderClient mLocationProviderClient;
    @Captor private ArgumentCaptor<LocationListener> mLocationListenerCaptor;
    @Captor private ArgumentCaptor<LocationRequest> mLocationRequestCaptor;

    @Before
    public void setUp() {
        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        GeolocationTracker.setLocationAgeForTesting(null);
        GeolocationHeader.setAppPermissionGrantedForTesting(true);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getProfile()).thenReturn(mProfileMock);
        when(mTab.getWebContents()).thenReturn(mWebContentsMock);
        when(mWebsitePreferenceBridgeJniMock.getPermissionSettingForOrigin(
                        any(BrowserContextHandle.class), eq(ContentSettingsType.GEOLOCATION),
                        anyString(), anyString()))
                .thenReturn(ContentSettingValues.ALLOW);
        when(mWebsitePreferenceBridgeJniMock.isDSEOrigin(
                        any(BrowserContextHandle.class), anyString()))
                .thenReturn(true);
        when(mUrlUtilitiesJniMock.isGoogleSearchUrl(anyString())).thenReturn(true);
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mTemplateUrlServiceMock.getUrlForSearchQuery(anyString()))
                .thenReturn("https://example.com/");
        sRefreshLastKnownLocation = 0;
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
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
        assertEquals("X-Geo: w " + ENCODED_PROTO_LOCATION, header);
    }

    @Test
    public void testGetGeoHeaderOld() {
        checkOldLocation("X-Geo: w " + ENCODED_PROTO_LOCATION);
    }

    @Test
    public void testGetGeoHeaderOldLocationAppPermissionDenied() {
        GeolocationHeader.setAppPermissionGrantedForTesting(false);
        // Nothing should be included when app permission is missing.
        checkOldLocation(null);
    }

    @Test
    @Config(shadows = {ShadowGeolocationTracker.class})
    public void testPrimeLocationForGeoHeader() {
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(1, sRefreshLastKnownLocation);
    }

    @Test
    @Config(shadows = {ShadowGeolocationTracker.class})
    public void testPrimeLocationForGeoHeaderPermissionOff() {
        GeolocationHeader.setAppPermissionGrantedForTesting(false);
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(0, sRefreshLastKnownLocation);
    }

    @Test
    @Config(shadows = {ShadowGeolocationTracker.class})
    public void testPrimeLocationForGeoHeaderDseAutograntOff() {
        when(mWebsitePreferenceBridgeJniMock.getPermissionSettingForOrigin(
                        any(BrowserContextHandle.class), eq(ContentSettingsType.GEOLOCATION),
                        anyString(), anyString()))
                .thenReturn(ContentSettingValues.ASK);
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(0, sRefreshLastKnownLocation);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.USE_FUSED_LOCATION_PROVIDER)
    @Config(shadows = {ShadowGeolocationTracker.class, ShadowLocationServices.class})
    public void testFusedLocationProvider() {
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        verify(mLocationProviderClient)
                .requestLocationUpdates(
                        mLocationRequestCaptor.capture(),
                        mLocationListenerCaptor.capture(),
                        eq(null));

        LocationRequest actualRequest = mLocationRequestCaptor.getValue();
        assertEquals(GeolocationHeader.REFRESH_LOCATION_AGE, actualRequest.getMaxUpdateAgeMillis());
        assertEquals(
                GeolocationHeader.LOCATION_REQUEST_UPDATE_INTERVAL,
                actualRequest.getMinUpdateIntervalMillis());
        assertEquals(Granularity.GRANULARITY_PERMISSION_LEVEL, actualRequest.getGranularity());

        Location mockLocation = generateMockLocation("network", LOCATION_TIME);
        mLocationListenerCaptor.getValue().onLocationChanged(mockLocation);
        assertEquals(mockLocation, GeolocationHeader.getLastKnownLocation());
        assertEquals(0, sRefreshLastKnownLocation);

        GeolocationHeader.stopListeningForLocationUpdates();
        verify(mLocationProviderClient).removeLocationUpdates(mLocationListenerCaptor.getValue());

        doThrow(new RuntimeException())
                .when(mLocationProviderClient)
                .requestLocationUpdates(
                        any(LocationRequest.class), any(LocationListener.class), eq(null));
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);

        assertEquals(1, sRefreshLastKnownLocation);
    }

    private void checkOldLocation(String expectedHeader) {
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        GeolocationTracker.setLocationForTesting(location, null);
        // 6 minutes should hit the age limit, but the feature is off.
        GeolocationTracker.setLocationAgeForTesting(6 * 60 * 1000L);
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
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

    /** Shadow for GeolocationTracker */
    @Implements(GeolocationTracker.class)
    public static class ShadowGeolocationTracker {
        @Implementation
        public static void refreshLastKnownLocation(Context context, long maxAge) {
            sRefreshLastKnownLocation++;
        }
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
