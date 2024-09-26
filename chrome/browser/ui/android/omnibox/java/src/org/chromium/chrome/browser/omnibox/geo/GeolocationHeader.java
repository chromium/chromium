// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.Manifest;
import android.content.pm.PackageManager;
import android.location.Location;
import android.net.Uri;
import android.os.Process;
import android.util.Base64;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.Granularity;
import com.google.android.gms.location.LocationListener;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationServices;
import com.google.android.gms.location.Priority;

import org.jni_zero.CalledByNative;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Duration;

/**
 * Provides methods for building the X-Geo HTTP header, which provides device location to a server
 * when making an HTTP request.
 *
 * <p>X-Geo header spec: https://goto.google.com/xgeospec.
 */
public class GeolocationHeader {
    @IntDef({
        HeaderState.HEADER_ENABLED,
        HeaderState.INCOGNITO,
        HeaderState.UNSUITABLE_URL,
        HeaderState.NOT_HTTPS,
        HeaderState.LOCATION_PERMISSION_BLOCKED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface HeaderState {
        int HEADER_ENABLED = 0;
        int INCOGNITO = 1;
        int UNSUITABLE_URL = 2;
        int NOT_HTTPS = 3;
        int LOCATION_PERMISSION_BLOCKED = 4;
    }

    /** The maximum age in milliseconds of a location that we'll send in an X-Geo header. */
    private static final int MAX_LOCATION_AGE = 24 * 60 * 60 * 1000; // 24 hours

    /** The maximum age in milliseconds of a location before we'll request a refresh. */
    @VisibleForTesting static final int REFRESH_LOCATION_AGE = 5 * 60 * 1000; // 5 minutes

    // 9 minutes is just below the 10 minute threshold to be considered fresh by the search backend.
    @VisibleForTesting
    static final long LOCATION_REQUEST_UPDATE_INTERVAL = Duration.ofMinutes(9).toMillis();

    /** The X-Geo header prefix, preceding any location descriptors */
    private static final String XGEO_HEADER_PREFIX = "X-Geo:";

    /**
     * The location descriptor separator used in the X-Geo header to separate encoding prefix, and
     * encoded descriptors
     */
    private static final String LOCATION_SEPARATOR = " ";

    /** The location descriptor prefix used in the X-Geo header to specify a proto wire encoding */
    private static final String LOCATION_PROTO_PREFIX = "w";

    private static final String DUMMY_URL_QUERY = "some_query";

    private static final LocationListener sLocationListener = GeolocationHeader::onLocationUpate;
    private static boolean sGeolocationPrimed;
    private static boolean sAppPermissionGrantedForTesting;
    private static boolean sUseAppPermissionGrantedForTesting;
    private static boolean sCurrentLocationRequested;
    private static Location sFusedLocation;

    /**
     * Requests a location refresh so that a valid location will be available for constructing an
     * X-Geo header in the near future (i.e. within 5 minutes). Checks whether the header can
     * actually be sent before requesting the location refresh. If UseFusedLocationProvider is true,
     * this starts listening for location updates on a regular interval rather than triggering a
     * single refresh.
     */
    public static void primeLocationForGeoHeaderIfEnabled(
            Profile profile, TemplateUrlService templateService) {
        if (profile == null) return;
        if (!hasGeolocationPermission()) return;
        if (!isGeoHeaderEnabledForDse(profile, templateService)) return;

        sGeolocationPrimed = true;

        boolean listeningForFusedLocationProviderUpdates =
                OmniboxFeatures.sUseFusedLocationProvider.isEnabled()
                        && startListeningForLocationUpdates();
        if (!listeningForFusedLocationProviderUpdates) {
            GeolocationTracker.refreshLastKnownLocation(
                    ContextUtils.getApplicationContext(), REFRESH_LOCATION_AGE);
        }
    }

    /**
     * Start listening for location updates on a LOCATION_REQUEST_UPDATE_INTERVAL interval,
     * returning true if the request to listen succeeded.
     *
     * <p>Locations are requested to be less than REFRESH_LOCATION_AGE minutes old and have a
     * granularity matching the app's permission level.
     */
    private static boolean startListeningForLocationUpdates() {
        if (sCurrentLocationRequested) return true;
        try (TraceEvent e =
                TraceEvent.scoped("GeolocationHeader.startListeningForLocationUpdates")) {
            FusedLocationProviderClient fusedLocationClient =
                    LocationServices.getFusedLocationProviderClient(
                            ContextUtils.getApplicationContext());

            long updateDuration =
                    Duration.ofMinutes(OmniboxFeatures.sGeolocationRequestTimeoutMinutes.getValue())
                            .toMillis();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        sCurrentLocationRequested = false;
                    },
                    updateDuration);
            var locationRequest =
                    new LocationRequest.Builder(LOCATION_REQUEST_UPDATE_INTERVAL)
                            .setDurationMillis(updateDuration)
                            .setMaxUpdateAgeMillis(REFRESH_LOCATION_AGE)
                            .setPriority(Priority.PRIORITY_BALANCED_POWER_ACCURACY)
                            .setGranularity(Granularity.GRANULARITY_PERMISSION_LEVEL)
                            .build();
            fusedLocationClient.requestLocationUpdates(locationRequest, sLocationListener, null);
            sCurrentLocationRequested = true;
        } catch (RuntimeException e) {
            // GMSCore may not exist on the device or be very old. Return false and trigger fallback
            // behavior.
            sCurrentLocationRequested = false;
        }
        return sCurrentLocationRequested;
    }

    /** Stop requesting and listening for location updates from FusedLocationProvider. */
    public static void stopListeningForLocationUpdates() {
        if (!sCurrentLocationRequested) return;
        FusedLocationProviderClient fusedLocationClient =
                LocationServices.getFusedLocationProviderClient(
                        ContextUtils.getApplicationContext());
        fusedLocationClient.removeLocationUpdates(sLocationListener);
        sCurrentLocationRequested = false;
    }

    private static void onLocationUpate(Location location) {
        sFusedLocation = location;
    }

    private static boolean isGeoHeaderEnabledForDse(
            Profile profile, TemplateUrlService templateService) {
        return geoHeaderStateForUrl(profile, templateService.getUrlForSearchQuery(DUMMY_URL_QUERY))
                == HeaderState.HEADER_ENABLED;
    }

    private static @HeaderState int geoHeaderStateForUrl(Profile profile, String url) {
        try (TraceEvent e = TraceEvent.scoped("GeolocationHeader.geoHeaderStateForUrl")) {
            // Only send X-Geo in normal mode.
            if (profile.isOffTheRecord()) return HeaderState.INCOGNITO;

            // Only send X-Geo header to Google domains.
            if (!UrlUtilitiesJni.get().isGoogleSearchUrl(url)) return HeaderState.UNSUITABLE_URL;

            Uri uri = Uri.parse(url);
            if (!UrlConstants.HTTPS_SCHEME.equals(uri.getScheme())) return HeaderState.NOT_HTTPS;

            if (!hasGeolocationPermission()) {
                return HeaderState.LOCATION_PERMISSION_BLOCKED;
            }

            // Only send X-Geo header if the user hasn't disabled geolocation for url.
            if (isLocationDisabledForUrl(profile, uri)) {
                return HeaderState.LOCATION_PERMISSION_BLOCKED;
            }

            return HeaderState.HEADER_ENABLED;
        }
    }

    /**
     * Returns an X-Geo HTTP header string if:
     *
     * <ul>
     *   <li>The current mode is not incognito,
     *   <li>The url is a google search URL (e.g. www.google.co.uk/search?q=cars),
     *   <li>The user has not disabled sharing location with this url, and
     *   <li>There is a valid and recent location available.
     * </ul>
     *
     * <p>Returns null otherwise.
     *
     * @param url The URL of the request with which this header will be sent.
     * @param tab The Tab currently being accessed.
     * @return The X-Geo header string or null.
     */
    public static @Nullable String getGeoHeader(String url, Tab tab) {
        return getGeoHeader(url, tab.getProfile());
    }

    /**
     * Returns an X-Geo HTTP header string if:
     *
     * <ul>
     *   <li>The current mode is not incognito,
     *   <li>The url is a google search URL (e.g. www.google.co.uk/search?q=cars),
     *   <li>The user has not disabled sharing location with this url, and
     *   <li>There is a valid and recent location available.
     * </ul>
     *
     * <p>Returns null otherwise.
     *
     * @param url The URL of the request with which this header will be sent.
     * @param profile The user profile being accessed.
     * @return The X-Geo header string or null.
     */
    @CalledByNative
    private static @Nullable String getGeoHeader(String url, Profile profile) {
        if (profile == null) return null;
        try (TraceEvent e = TraceEvent.scoped("GeolocationHeader.getGeoHeader")) {
            Location locationToAttach = null;
            long locationAge = Long.MAX_VALUE;
            @HeaderState int headerState = geoHeaderStateForUrl(profile, url);
            if (headerState == HeaderState.HEADER_ENABLED) {
                locationToAttach = getLastKnownLocation();
                if (locationToAttach != null) {
                    locationAge = GeolocationTracker.getLocationAge(locationToAttach);
                    if (locationAge > MAX_LOCATION_AGE) {
                        // Do not attach the location
                        locationToAttach = null;
                    }
                }
            }

            // Proto encoding
            String locationProtoEncoding = encodeProtoLocation(locationToAttach);
            if (locationProtoEncoding == null) return null;

            StringBuilder header = new StringBuilder(XGEO_HEADER_PREFIX);
            if (locationProtoEncoding != null) {
                header.append(LOCATION_SEPARATOR)
                        .append(LOCATION_PROTO_PREFIX)
                        .append(LOCATION_SEPARATOR)
                        .append(locationProtoEncoding);
            }
            return header.toString();
        }
    }

    @CalledByNative
    private static boolean hasGeolocationPermission() {
        if (sUseAppPermissionGrantedForTesting) return sAppPermissionGrantedForTesting;
        int pid = Process.myPid();
        int uid = Process.myUid();
        if (ApiCompatibilityUtils.checkPermission(
                        ContextUtils.getApplicationContext(),
                        Manifest.permission.ACCESS_COARSE_LOCATION,
                        pid,
                        uid)
                != PackageManager.PERMISSION_GRANTED) {
            return false;
        }
        return true;
    }

    /**
     * Returns true if the user has disabled sharing their location with url (e.g. via the
     * geolocation infobar).
     */
    private static boolean isLocationDisabledForUrl(Profile profile, Uri uri) {
        // TODO(raymes): The call to isDseOrigin is only needed if this could be called for
        // an origin that isn't the default search engine. Otherwise remove this line.
        boolean isDseOrigin = WebsitePreferenceBridge.isDSEOrigin(profile, uri.toString());
        @ContentSettingValues
        @Nullable
        Integer settingValue = locationContentSettingForUrl(profile, uri);

        boolean enabled = isDseOrigin && settingValue == ContentSettingValues.ALLOW;
        return !enabled;
    }

    /**
     * Returns the location permission for sharing their location with url (e.g. via the geolocation
     * infobar).
     */
    private static @ContentSettingValues @Nullable Integer locationContentSettingForUrl(
            Profile profile, Uri uri) {
        return PermissionInfo.getContentSetting(
                profile, ContentSettingsType.GEOLOCATION, uri.toString(), null);
    }

    static void setAppPermissionGrantedForTesting(boolean appPermissionGrantedForTesting) {
        sAppPermissionGrantedForTesting = appPermissionGrantedForTesting;
        sUseAppPermissionGrantedForTesting = true;
    }

    static boolean isGeolocationPrimedForTesting() {
        return sGeolocationPrimed;
    }

    @VisibleForTesting
    static Location getLastKnownLocation() {
        if (OmniboxFeatures.sUseFusedLocationProvider.isEnabled() && sFusedLocation != null) {
            return sFusedLocation;
        }
        return GeolocationTracker.getLastKnownLocation(ContextUtils.getApplicationContext());
    }

    /** Encodes location into proto encoding. */
    @Nullable
    @VisibleForTesting
    static String encodeProtoLocation(@Nullable Location location) {
        if (location == null) return null;

        // Timestamp in microseconds since the UNIX epoch.
        long timestamp = location.getTime() * 1000;
        // Latitude times 1e7.
        int latitudeE7 = (int) (location.getLatitude() * 10000000);
        // Longitude times 1e7.
        int longitudeE7 = (int) (location.getLongitude() * 10000000);
        // Radius of 68% accuracy in mm.
        int radius = (int) (location.getAccuracy() * 1000);

        // Create a LatLng for the coordinates.
        PartnerLocationDescriptor.LatLng latlng =
                PartnerLocationDescriptor.LatLng.newBuilder()
                        .setLatitudeE7(latitudeE7)
                        .setLongitudeE7(longitudeE7)
                        .build();

        // Populate a LocationDescriptor with the LatLng.
        PartnerLocationDescriptor.LocationDescriptor locationDescriptor =
                PartnerLocationDescriptor.LocationDescriptor.newBuilder()
                        .setLatlng(latlng)
                        // Include role, producer, timestamp and radius.
                        .setRole(PartnerLocationDescriptor.LocationRole.CURRENT_LOCATION)
                        .setProducer(PartnerLocationDescriptor.LocationProducer.DEVICE_LOCATION)
                        .setTimestamp(timestamp)
                        .setRadius((float) radius)
                        .build();
        return encodeLocationDescriptor(locationDescriptor);
    }

    /** Encodes the given proto location descriptor into a BASE64 URL_SAFE encoding. */
    private static String encodeLocationDescriptor(
            PartnerLocationDescriptor.LocationDescriptor locationDescriptor) {
        return Base64.encodeToString(
                locationDescriptor.toByteArray(), Base64.NO_WRAP | Base64.URL_SAFE);
    }
}
