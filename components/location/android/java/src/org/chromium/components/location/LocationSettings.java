// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.location;

import android.Manifest;

import org.jni_zero.CalledByNative;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/** Provides native access to system-level location settings and permissions. */
@NullMarked
public class LocationSettings {
    private LocationSettings() {}

    @CalledByNative
    private static boolean hasAndroidLocationPermission() {
        return LocationUtils.getInstance().hasAndroidLocationPermission();
    }

    @CalledByNative
    private static boolean hasAndroidFineLocationPermission() {
        return LocationUtils.getInstance().hasAndroidFineLocationPermission();
    }

    @CalledByNative
    private static boolean canPromptForAndroidLocationPermission(WindowAndroid windowAndroid) {
        // TODO(crbug.com/40765216): Investigate if this should be ACCESS_COARSE_LOCATION.
        return windowAndroid.canRequestPermission(Manifest.permission.ACCESS_FINE_LOCATION);
    }

    @CalledByNative
    private static boolean isSystemLocationSettingEnabled() {
        return LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    @CalledByNative
    private static boolean canPromptToEnableSystemLocationSetting() {
        return LocationUtils.getInstance().canPromptToEnableSystemLocationSetting();
    }

    @CalledByNative
    private static void promptToEnableSystemLocationSetting(
            @LocationSettingsDialogContext int promptContext,
            WindowAndroid window,
            JniOnceCallback<Integer> callback) {
        LocationUtils.getInstance()
                .promptToEnableSystemLocationSetting(promptContext, window, callback);
    }
}
