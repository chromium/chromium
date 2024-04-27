// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.os.Build;
import android.os.Handler;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

/**
 * Manager for Cast settings.
 */
public final class CastSettingsManager {
    private static final String TAG = "CastSettingsManager";

    private static final String PREFS_FILE_NAME = "CastSettings";

    /** The default device name, which is the model name. */
    private static final String DEFAULT_DEVICE_NAME = Build.MODEL;

    private static final String DEVICE_NAME_SETTING_KEY = Settings.Global.DEVICE_NAME;
    private static final String DEVICE_PROVISIONED_SETTING_KEY = Settings.Global.DEVICE_PROVISIONED;

    private final ContentResolver mContentResolver;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    ContentObserver mDeviceNameObserver;
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    ContentObserver mIsDeviceProvisionedObserver;

    /**
     * Can be implemented to receive notifications from a CastSettingsManager instance when
     * settings have changed.
     */
    public static class OnSettingChangedListener {
        public void onCastEnabledChanged(boolean enabled) {}
        public void onDeviceNameChanged(String deviceName) {}
    }

    private OnSettingChangedListener mListener;

    /**
     * Creates a fully-featured CastSettingsManager instance. Will fail if called from a
     * sandboxed process.
     */
    public static CastSettingsManager createCastSettingsManager(
            Context context, OnSettingChangedListener listener) {
        ContentResolver contentResolver = context.getContentResolver();
        return new CastSettingsManager(contentResolver, listener);
    }

    private CastSettingsManager(
            ContentResolver contentResolver, OnSettingChangedListener listener) {
        mContentResolver = contentResolver;
        mListener = listener;

        mDeviceNameObserver =
                new ContentObserver(new Handler()) {
                    @Override
                    public void onChange(boolean selfChange) {
                        mListener.onDeviceNameChanged(getDeviceName());
                    }
                };
        // TODO(crbug.com/40479664): Fix lint properly.
        mContentResolver.registerContentObserver(
                Settings.Global.getUriFor(DEVICE_NAME_SETTING_KEY), true, mDeviceNameObserver);

        if (!isCastEnabled()) {
            mIsDeviceProvisionedObserver =
                    new ContentObserver(new Handler()) {
                        @Override
                        public void onChange(boolean selfChange) {
                            Log.d(TAG, "Device provisioned");
                            mListener.onCastEnabledChanged(isCastEnabled());
                        }
                    };
            // TODO(crbug.com/40479664): Fix lint properly.
            mContentResolver.registerContentObserver(
                    Settings.Global.getUriFor(DEVICE_PROVISIONED_SETTING_KEY),
                    true,
                    mIsDeviceProvisionedObserver);
        }
    }

    public void dispose() {
        mContentResolver.unregisterContentObserver(mDeviceNameObserver);
        mDeviceNameObserver = null;

        if (mIsDeviceProvisionedObserver != null) {
            mContentResolver.unregisterContentObserver(mIsDeviceProvisionedObserver);
            mIsDeviceProvisionedObserver = null;
        }
    }

    public boolean isCastEnabled() {
        // However, Cast is disabled until the device is provisioned (see b/18950240).
        // TODO(crbug.com/40479664): Fix lint properly.
        return Settings.Global.getInt(mContentResolver, DEVICE_PROVISIONED_SETTING_KEY, 0) == 1;
    }

    public String getDeviceName() {
        // TODO(crbug.com/40479664): Fix lint properly.
        String deviceName = Settings.Global.getString(mContentResolver, DEVICE_NAME_SETTING_KEY);
        return (deviceName != null) ? deviceName : DEFAULT_DEVICE_NAME;
    }
}
