// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.location.Location;
import android.os.SystemClock;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleWifi;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.HashSet;

/**
 * Robolectric tests for {@link GeolocationHeader}.
 */
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

    private static final VisibleWifi VISIBLE_WIFI1 =
            VisibleWifi.create("ssid1", "11:11:11:11:11:11", -1, 10L);
    private static final VisibleWifi VISIBLE_WIFI_NO_LEVEL =
            VisibleWifi.create("ssid1", "11:11:11:11:11:11", null, 10L);
    private static final VisibleWifi VISIBLE_WIFI2 =
            VisibleWifi.create("ssid2", "11:11:11:11:11:12", -10, 20L);
    private static final VisibleWifi VISIBLE_WIFI3 =
            VisibleWifi.create("ssid3", "11:11:11:11:11:13", -30, 30L);
    private static final VisibleWifi VISIBLE_WIFI_NOMAP =
            VisibleWifi.create("ssid1_nomap", "11:11:11:11:11:11", -1, 10L);
    private static final VisibleWifi VISIBLE_WIFI_OPTOUT =
            VisibleWifi.create("ssid1_optout", "11:11:11:11:11:11", -1, 10L);
    private static final VisibleCell VISIBLE_CELL1 = VisibleCell.builder(VisibleCell.RadioType.CDMA)
                                                             .setCellId(10)
                                                             .setLocationAreaCode(11)
                                                             .setMobileCountryCode(12)
                                                             .setMobileNetworkCode(13)
                                                             .setTimestamp(10L)
                                                             .build();
    private static final VisibleCell VISIBLE_CELL2 = VisibleCell.builder(VisibleCell.RadioType.GSM)
                                                             .setCellId(20)
                                                             .setLocationAreaCode(21)
                                                             .setMobileCountryCode(22)
                                                             .setMobileNetworkCode(23)
                                                             .setTimestamp(20L)
                                                             .build();
    // Encoded proto location for VISIBLE_WIFI1 connected, VISIBLE_WIFI3 not connected,
    // VISIBLE_CELL1 connected, VISIBLE_CELL2 not connected.
    private static final String ENCODED_PROTO_VISIBLE_NETWORKS =
            "CAEQDLoBJAoeChExMToxMToxMToxMToxMToxMRD___________8BGAEgCroBJAoeChExMToxMToxMToxMTox"
            + "MToxMxDi__________8BGAAgHroBEBIKCAMQChgLIAwoDRgBIAq6ARASCggBEBQYFSAWKBcYACAU";

    private static int sRefreshVisibleNetworksRequests;
    private static int sRefreshLastKnownLocation;

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();


    @Mock
    UrlUtilities.Natives mUrlUtilitiesJniMock;

    @Mock
    WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

    @Mock
    Profile.Natives mProfileJniMock;

    @Mock
    Profile mProfileMock;

    @Mock
    private Tab mTab;

    @Mock
    WebContents mWebContentsMock;

    @Mock
    TemplateUrlService mTemplateUrlServiceMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        mocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);
        GeolocationTracker.setLocationAgeForTesting(null);
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LocationSource.HIGH_ACCURACY);
        GeolocationHeader.setAppPermissionGrantedForTesting(true);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(mWebContentsMock);
        when(mWebsitePreferenceBridgeJniMock.getPermissionSettingForOrigin(
                     any(BrowserContextHandle.class), eq(ContentSettingsType.GEOLOCATION),
                     anyString(), anyString()))
                .thenReturn(ContentSettingValues.ALLOW);
        when(mWebsitePreferenceBridgeJniMock.isDSEOrigin(
                     any(BrowserContextHandle.class), anyString()))
                .thenReturn(true);
        when(mUrlUtilitiesJniMock.isGoogleSearchUrl(anyString())).thenReturn(true);
        when(mProfileJniMock.fromWebContents(any(WebContents.class))).thenReturn(mProfileMock);
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mTemplateUrlServiceMock.getUrlForSearchQuery(anyString()))
                .thenReturn("https://example.com/");
        sRefreshVisibleNetworksRequests = 0;
        sRefreshLastKnownLocation = 0;
    }

    @Test
    public void testEncodeProtoLocation() {
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        String encodedProtoLocation = GeolocationHeader.encodeProtoLocation(location);
        assertEquals(ENCODED_PROTO_LOCATION, encodedProtoLocation);
    }

    @Test
    public void voidtestTrimVisibleNetworks() {
        VisibleNetworks visibleNetworks =
                VisibleNetworks.create(VISIBLE_WIFI_NO_LEVEL, VISIBLE_CELL1,
                        new HashSet<>(Arrays.asList(VISIBLE_WIFI1, VISIBLE_WIFI2, VISIBLE_WIFI3)),
                        new HashSet<>(Arrays.asList(VISIBLE_CELL1, VISIBLE_CELL2)));

        // We expect trimming to replace connected Wifi (since it will have level), and select only
        // the visible wifi different from the connected one, with strongest level.
        VisibleNetworks expectedTrimmed = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet<>(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet<>(Arrays.asList(VISIBLE_CELL2)));

        VisibleNetworks trimmed = GeolocationHeader.trimVisibleNetworks(visibleNetworks);
        assertEquals(expectedTrimmed, trimmed);
    }

    @Test
    public void testTrimVisibleNetworksEmptyOrNull() {
        VisibleNetworks visibleNetworks =
                VisibleNetworks.create(VisibleWifi.create("whatever", null, null, null), null,
                        new HashSet<>(), new HashSet<>());
        assertNull(GeolocationHeader.trimVisibleNetworks(visibleNetworks));
        assertNull(GeolocationHeader.trimVisibleNetworks(null));
    }

    @Test
    public void testEncodeProtoVisibleNetworks() {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet<>(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet<>(Arrays.asList(VISIBLE_CELL2)));
        String encodedProtoLocation = GeolocationHeader.encodeProtoVisibleNetworks(visibleNetworks);
        assertEquals(ENCODED_PROTO_VISIBLE_NETWORKS, encodedProtoLocation);
    }

    @Test
    public void testEncodeProtoVisibleNetworksEmptyOrNull() {
        assertNull(GeolocationHeader.encodeProtoVisibleNetworks(null));
        assertNull(GeolocationHeader.encodeProtoVisibleNetworks(
                VisibleNetworks.create(null, null, null, null)));
        assertNull(GeolocationHeader.encodeProtoVisibleNetworks(
                VisibleNetworks.create(null, null, new HashSet<>(), new HashSet<>())));
        assertNotNull(GeolocationHeader.encodeProtoVisibleNetworks(VisibleNetworks.create(
                null, null, null, new HashSet<>(Arrays.asList(VISIBLE_CELL2)))));
    }

    @Test
    public void testEncodeProtoVisibleNetworksExcludeNoMapOrOptout() {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI_NOMAP, null,
                new HashSet<>(Arrays.asList(VISIBLE_WIFI_OPTOUT)), new HashSet<>());
        String encodedProtoLocation = GeolocationHeader.encodeProtoVisibleNetworks(visibleNetworks);
        assertNull(encodedProtoLocation);
    }

    @Test
    public void testGetGeoHeaderFreshLocation() {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet<>(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet<>(Arrays.asList(VISIBLE_CELL2)));
        VisibleNetworksTracker.setVisibleNetworksForTesting(visibleNetworks);
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        GeolocationTracker.setLocationForTesting(location, null);
        // 1 minute should be good enough and not require visible networks.
        GeolocationTracker.setLocationAgeForTesting(1 * 60 * 1000L);
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
        assertEquals("X-Geo: w " + ENCODED_PROTO_LOCATION, header);
    }

    @Test
    public void testGetGeoHeaderLocationMissing() {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet<>(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet<>(Arrays.asList(VISIBLE_CELL2)));
        VisibleNetworksTracker.setVisibleNetworksForTesting(visibleNetworks);
        GeolocationTracker.setLocationForTesting(null, null);
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
        assertEquals("X-Geo: w " + ENCODED_PROTO_VISIBLE_NETWORKS, header);
    }

    @Test
    public void testGetGeoHeaderOldLocationHighAccuracy() {
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LocationSource.HIGH_ACCURACY);
        // Visible networks should be included
        checkOldLocation(
                "X-Geo: w " + ENCODED_PROTO_LOCATION + " w " + ENCODED_PROTO_VISIBLE_NETWORKS);
    }

    @Test
    public void testGetGeoHeaderOldLocationBatterySaving() {
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LocationSource.BATTERY_SAVING);
        checkOldLocation(
                "X-Geo: w " + ENCODED_PROTO_LOCATION + " w " + ENCODED_PROTO_VISIBLE_NETWORKS);
    }

    @Test
    public void testGetGeoHeaderOldLocationGpsOnly() {
        GeolocationHeader.setLocationSourceForTesting(GeolocationHeader.LocationSource.GPS_ONLY);
        // In GPS only mode, networks should never be included.
        checkOldLocation("X-Geo: w " + ENCODED_PROTO_LOCATION);
    }

    @Test
    public void testGetGeoHeaderOldLocationLocationOff() {
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LocationSource.LOCATION_OFF);
        // If the location switch is off, networks should never be included (old location might).
        checkOldLocation("X-Geo: w " + ENCODED_PROTO_LOCATION);
    }

    @Test
    public void testGetGeoHeaderOldLocationAppPermissionDenied() {
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LocationSource.HIGH_ACCURACY);
        GeolocationHeader.setAppPermissionGrantedForTesting(false);
        // Nothing should be included when app permission is missing.
        checkOldLocation(null);
    }

    @Test
    public void testGetGeoHeaderNoProfile() {
        when(mProfileJniMock.fromWebContents(any(WebContents.class))).thenReturn(null);
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
        assertNull(header);
    }

    @Test
    @Config(shadows = {ShadowVisibleNetworksTracker.class, ShadowGeolocationTracker.class})
    public void testPrimeLocationForGeoHeader() {
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(1, sRefreshLastKnownLocation);
        assertEquals(1, sRefreshVisibleNetworksRequests);
    }

    @Test
    @Config(shadows = {ShadowVisibleNetworksTracker.class, ShadowGeolocationTracker.class})
    public void testPrimeLocationForGeoHeaderPermissionOff() {
        GeolocationHeader.setAppPermissionGrantedForTesting(false);
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(0, sRefreshLastKnownLocation);
        assertEquals(0, sRefreshVisibleNetworksRequests);
    }

    @Test
    @Config(shadows = {ShadowVisibleNetworksTracker.class, ShadowGeolocationTracker.class})
    public void testPrimeLocationForGeoHeaderDSEAutograntOff() {
        when(mWebsitePreferenceBridgeJniMock.getPermissionSettingForOrigin(
                     any(BrowserContextHandle.class), eq(ContentSettingsType.GEOLOCATION),
                     anyString(), anyString()))
                .thenReturn(ContentSettingValues.ASK);
        GeolocationHeader.primeLocationForGeoHeaderIfEnabled(mProfileMock, mTemplateUrlServiceMock);
        assertEquals(0, sRefreshLastKnownLocation);
        assertEquals(0, sRefreshVisibleNetworksRequests);
    }

    private void checkOldLocation(String expectedHeader) {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet<>(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet<>(Arrays.asList(VISIBLE_CELL2)));
        VisibleNetworksTracker.setVisibleNetworksForTesting(visibleNetworks);
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

    /**
     * Shadow for VisibleNetworksTracker
     */
    @Implements(VisibleNetworksTracker.class)
    public static class ShadowVisibleNetworksTracker {
        @Implementation
        public static void refreshVisibleNetworks(final Context context) {
            sRefreshVisibleNetworksRequests++;
        }
    }

    /**
     * Shadow for GeolocationTracker
     */
    @Implements(GeolocationTracker.class)
    public static class ShadowGeolocationTracker {
        @Implementation
        public static void refreshLastKnownLocation(Context context, long maxAge) {
            sRefreshLastKnownLocation++;
        }
    }
}
