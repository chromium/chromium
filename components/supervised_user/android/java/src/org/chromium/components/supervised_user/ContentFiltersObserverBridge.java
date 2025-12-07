// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.supervised_user;

import android.database.ContentObserver;
import android.os.Handler;
import android.provider.Settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A {@link ContentFiltersObserverBridge} creates Android observer for the content filters settings
 * that can be used by the native code. The value of the setting is read from the secure settings
 * storage, triggered either by asynchronous updates or by application state changes.
 *
 * <p>The native code creates and manages lifecycle of an instance of this class and is notified
 * when: 1. the value of settings is read synchronously for the first time, 2. when the secure
 * settings storage asynchronously delivers update and the value of the setting is different from
 * the previous value, or 3. when the application state changes and the value of the setting is
 * different from the previous value.
 */
@NullMarked
@JNINamespace("supervised_user")
class ContentFiltersObserverBridge {

    // Supervised User Content Filters Observer.
    private static final String TAG = "SUCFiltersObserver";

    // Notified asynchronously from secure settings.
    private final ContentObserver mObserver;
    // Triggered when application state changes, to read the secure settings synchronously.
    private final ApplicationStatus.ApplicationStateListener mStateListener;
    // Caches the logical value of the setting to avoid calling the native code with the same value.
    private @Nullable Boolean mIsEnabled;

    /**
     * @param nativeContentFiltersObserverBridge The native bridge.
     * @param settingName The name of the setting to observe.
     */
    @CalledByNative
    private ContentFiltersObserverBridge(
            long nativeContentFiltersObserverBridge, String settingName) {
        mObserver =
                new ContentObserver(
                        new Handler(ContextUtils.getApplicationContext().getMainLooper())) {
                    @Override
                    public void onChange(boolean selfChange) {
                        updateFromSecureSettings(nativeContentFiltersObserverBridge, settingName);
                    }
                };
        mStateListener =
                new ApplicationStatus.ApplicationStateListener() {
                    @Override
                    public void onApplicationStateChange(@ApplicationState int newState) {
                        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
                            updateFromSecureSettings(
                                    nativeContentFiltersObserverBridge, settingName);
                        }
                    }
                };

        ContextUtils.getApplicationContext()
                .getContentResolver()
                .registerContentObserver(
                        Settings.Secure.getUriFor(settingName),
                        /* notifyForDescendants= */ false,
                        mObserver);
        ApplicationStatus.registerApplicationStateListener(mStateListener);

        updateFromSecureSettings(nativeContentFiltersObserverBridge, settingName);
    }

    /**
     * Updates internal state synchronously from secure settings. If the value of setting changed or
     * upon the first read, then the native code is notified.
     */
    private void updateFromSecureSettings(
            long nativeContentFiltersObserverBridge, String settingName) {
        boolean newEnabled = getValue(settingName);

        if (mIsEnabled != null && mIsEnabled == newEnabled) {
            Log.i(TAG, "setting=%s discarding %s", settingName, newEnabled);
            return;
        }

        mIsEnabled = newEnabled;

        ContentFiltersObserverBridgeJni.get()
                .onChange(nativeContentFiltersObserverBridge, mIsEnabled);
        Log.i(TAG, "setting=%s updating with %s", settingName, newEnabled);
    }

    @CalledByNative
    private void destroy() {
        ContextUtils.getApplicationContext()
                .getContentResolver()
                .unregisterContentObserver(mObserver);
        ApplicationStatus.unregisterApplicationStateListener(mStateListener);
    }

    private static boolean getValue(final String settingsName) {
        try {
            // The setting is considered enabled if the setting's value is positive.
            return Settings.Secure.getInt(
                            ContextUtils.getApplicationContext().getContentResolver(), settingsName)
                    > 0;
        } catch (Settings.SettingNotFoundException e) {
            // If the setting is not found, we assume it is disabled.
            return false;
        }
    }

    @NativeMethods
    interface Natives {
        void onChange(long nativeContentFiltersObserverBridge, boolean enabled);
    }
}
