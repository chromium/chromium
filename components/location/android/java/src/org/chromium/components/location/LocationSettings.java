// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.location;

import android.Manifest;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;

/** Provides native access to system-level location settings and permissions. */
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
            final long nativeCallback) {
        LocationUtils.getInstance()
                .promptToEnableSystemLocationSetting(
                        promptContext,
                        window,
                        new Callback<Integer>() {
                            @Override
                            public void onResult(Integer result) {
                                LocationSettingsJni.get()
                                        .onLocationSettingsDialogOutcome(nativeCallback, result);
                            }
                        });
    }

    @NativeMethods
    interface Natives {
        void onLocationSettingsDialogOutcome(long callback, int result);
    }
}
