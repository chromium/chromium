// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;

/**
 * Keeps track of the device's location, allowing synchronous location requests.
 * getLastKnownLocation() returns the current best estimate of the location. If possible, call
 * refreshLastKnownLocation() several seconds before a location is needed to maximize the chances
 * that the location is known.
 */
class GeolocationTracker {

    private static SelfCancelingListener sListener;
    private static Location sNetworkLocationForTesting;
    private static Location sGpsLocationForTesting;
    private static boolean sUseLocationForTesting;
    private static long sLocationAgeForTesting;
    private static boolean sUseLocationAgeForTesting;

    private static class SelfCancelingListener implements LocationListener {

        // Length of time before the location request should be canceled. This timeout ensures the
        // device doesn't get stuck in an infinite loop trying and failing to get a location, which
        // would cause battery drain. See: http://crbug.com/309917
        private static final int REQUEST_TIMEOUT_MS = 60 * 1000;  // 60 sec.

        private final LocationManager mLocationManager;
        private final Handler mHandler;
        private final Runnable mCancelRunnable;

        private boolean mRegistrationFailed;

        private SelfCancelingListener(LocationManager manager) {
            mLocationManager = manager;
            mHandler = new Handler();
            mCancelRunnable = new Runnable() {
                @Override
                public void run() {
                    try {
                        mLocationManager.removeUpdates(SelfCancelingListener.this);
                    } catch (Exception e) {
                        if (!mRegistrationFailed) throw e;
                    }
                    sListener = null;
                }
            };
            mHandler.postDelayed(mCancelRunnable, REQUEST_TIMEOUT_MS);
        }

        private void markRegistrationFailed() {
            mRegistrationFailed = true;
        }

        @Override
        public void onLocationChanged(Location location) {
            mHandler.removeCallbacks(mCancelRunnable);
            sListener = null;
        }

        @Override
        public void onProviderDisabled(String provider) { }

        @Override
        public void onProviderEnabled(String provider) { }

        @Override
        public void onStatusChanged(String provider, int status, Bundle extras) { }
    }

    /**
     * Returns the age of location is milliseconds.
     * Note: the age will be invalid if the system clock has been changed since the location was
     * created. If the apparent age is negative, Long.MAX_VALUE will be returned.
     */
    static long getLocationAge(Location location) {
        if (sUseLocationAgeForTesting) return sLocationAgeForTesting;
        long age = System.currentTimeMillis() - location.getTime();
        return age >= 0 ? age : Long.MAX_VALUE;
    }

    /**
     * Returns the last known location or null if none is available.
     */
    static Location getLastKnownLocation(Context context) {
        if (sUseLocationForTesting) {
            return chooseLocation(sNetworkLocationForTesting, sGpsLocationForTesting);
        }

        if (!hasPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION)) {
            // Do not call location manager without permissions
            return null;
        }

        LocationManager locationManager =
                (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        Location networkLocation =
                locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER);
        Location gpsLocation = null;
        if (hasPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)) {
            // Only try to get GPS location when ACCESS_FINE_LOCATION is granted.
            gpsLocation = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER);
        }
        return chooseLocation(networkLocation, gpsLocation);
    }

    /**
     * Requests an updated location if the last known location is older than maxAge milliseconds.
     *
     * Note: this must be called only on the UI thread.
     */
    static void refreshLastKnownLocation(Context context, long maxAge) {
        ThreadUtils.assertOnUiThread();

        if (!hasPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION)) {
            return;
        }

        // We're still waiting for a location update.
        if (sListener != null) return;

        LocationManager locationManager =
                (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        Location location = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER);
        if (location == null || getLocationAge(location) > maxAge) {
            String provider = LocationManager.NETWORK_PROVIDER;
            if (locationManager.isProviderEnabled(provider)) {
                sListener = new SelfCancelingListener(locationManager);
                try {
                    locationManager.requestSingleUpdate(provider, sListener, null);
                } catch (NullPointerException ex) {
                    // https://crbug.com/819730: This can trigger an NPE due to a underlying
                    // OS/framework bug.  By ignoring this, we will not get a newer location age.
                    sListener.markRegistrationFailed();
                }
            }
        }
    }

    @VisibleForTesting
    static void setLocationForTesting(
            Location networkLocationForTesting, Location gpsLocationForTesting) {
        sNetworkLocationForTesting = networkLocationForTesting;
        sGpsLocationForTesting = gpsLocationForTesting;
        sUseLocationForTesting = true;
    }

    @VisibleForTesting
    static void setLocationAgeForTesting(Long locationAgeForTesting) {
        if (locationAgeForTesting == null) {
            sUseLocationAgeForTesting = false;
            return;
        }
        sLocationAgeForTesting = locationAgeForTesting;
        sUseLocationAgeForTesting = true;
    }

    private static boolean hasPermission(Context context, String permission) {
        return ApiCompatibilityUtils.checkPermission(
                       context, permission, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    private static Location chooseLocation(Location networkLocation, Location gpsLocation) {
        if (gpsLocation == null) {
            return networkLocation;
        }

        if (networkLocation == null) {
            return gpsLocation;
        }

        // Both are not null, take the younger one.
        return networkLocation.getTime() > gpsLocation.getTime() ? networkLocation : gpsLocation;
    }
}
