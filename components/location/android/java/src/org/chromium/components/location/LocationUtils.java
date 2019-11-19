// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.location;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.os.Build;
import android.os.Process;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.compat.ApiHelperForP;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides methods for querying Chrome's ability to use Android's location services.
 *
 * This class should be used only on the UI thread.
 */
public class LocationUtils {
    // Used to construct sInstance if that's null.
    private static Factory sFactory;

    private static LocationUtils sInstance;

    protected LocationUtils() {}

    /**
     * Returns the singleton instance of LocationSettings, creating it if needed.
     */
    public static LocationUtils getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            if (sFactory == null) {
                sInstance = new LocationUtils();
            } else {
                sInstance = sFactory.create();
            }
        }
        return sInstance;
    }

    private boolean hasPermission(String name) {
        Context context = ContextUtils.getApplicationContext();
        return ApiCompatibilityUtils.checkPermission(
                context, name, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Returns true if Chromium has permission to access location.
     *
     * Check both hasAndroidLocationPermission() and isSystemLocationSettingEnabled() to determine
     * if Chromium's location requests will return results.
     */
    public boolean hasAndroidLocationPermission() {
        return hasPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                || hasPermission(Manifest.permission.ACCESS_FINE_LOCATION);
    }

    /**
     * Returns whether location services are enabled system-wide, i.e. whether any application is
     * able to access location.
     */
    @SuppressWarnings("deprecation")
    public boolean isSystemLocationSettingEnabled() {
        Context context = ContextUtils.getApplicationContext();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            LocationManager locationManager =
                    (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
            return locationManager != null && ApiHelperForP.isLocationEnabled(locationManager);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            return Settings.Secure.getInt(context.getContentResolver(),
                           Settings.Secure.LOCATION_MODE, Settings.Secure.LOCATION_MODE_OFF)
                    != Settings.Secure.LOCATION_MODE_OFF;
        } else {
            return !TextUtils.isEmpty(Settings.Secure.getString(
                    context.getContentResolver(), Settings.Secure.LOCATION_PROVIDERS_ALLOWED));
        }
    }

    /**
     * Returns whether location services are enabled in sensors-only mode, i.e. when network
     * location services are disabled but GPS and other sensors are enabled.
     */
    @SuppressWarnings("deprecation")
    public boolean isSystemLocationSettingSensorsOnly() {
        Context context = ContextUtils.getApplicationContext();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            LocationManager locationManager =
                    (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
            return locationManager != null && ApiHelperForP.isLocationEnabled(locationManager)
                    && locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)
                    && !locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            return Settings.Secure.getInt(context.getContentResolver(),
                           Settings.Secure.LOCATION_MODE, Settings.Secure.LOCATION_MODE_OFF)
                    == Settings.Secure.LOCATION_MODE_SENSORS_ONLY;
        } else {
            // Before Android K, location provider settings were stored as a comma-delimited list
            // containing the names of enabled providers. In sensors-only mode, the GPS provider is
            // present and the network provider is absent.
            String locationProviders = Settings.Secure.getString(
                    context.getContentResolver(), Settings.Secure.LOCATION_PROVIDERS_ALLOWED);
            return locationProviders.contains(LocationManager.GPS_PROVIDER)
                    && !locationProviders.contains(LocationManager.NETWORK_PROVIDER);
        }
    }

    /**
     * Returns true iff a prompt can be triggered to ask the user to turn on the system location
     * setting on their device.
     *
     * <p>In particular, returns false if the system location setting is already enabled or if some
     * of the features required to trigger a system location setting prompt are not available.
     */
    public boolean canPromptToEnableSystemLocationSetting() {
        return false;
    }

    /**
     * Triggers a prompt to ask the user to turn on the system location setting on their device.
     *
     * <p>The prompt will be triggered within the specified window.
     *
     * <p>The callback is guaranteed to be called unless the user never replies to the prompt
     * dialog, which in practice happens very infrequently since the dialog is modal.
     *
     * TODO(crbug/730711): Add back @LocationSettingsDialogOutcome to the callback when type
     *     annotations are allowed in Java 8.
     */
    public void promptToEnableSystemLocationSetting(
            @LocationSettingsDialogContext int promptContext, WindowAndroid window,
            Callback<Integer> callback) {
        callback.onResult(LocationSettingsDialogOutcome.NO_PROMPT);
    }

    /**
     * Returns an intent to launch Android Location Settings.
     */
    public Intent getSystemLocationSettingsIntent() {
        Intent i = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return i;
    }

    /**
     * Instantiate this to explain how to create a LocationUtils instance in
     * LocationUtils.getInstance().
     */
    public interface Factory { public LocationUtils create(); }

    /**
     * Call this to use a different subclass of LocationUtils throughout the program.
     * This can be used by embedders in addition to tests.
     */
    @VisibleForTesting
    public static void setFactory(Factory factory) {
        sFactory = factory;
        sInstance = null;
    }
}
