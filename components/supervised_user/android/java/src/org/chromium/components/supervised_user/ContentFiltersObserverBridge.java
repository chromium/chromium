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

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/**
 * A {@link ContentFiltersObserverBridge} creates Android observers for the content filters settings
 * that can be used by the native code. The native code creates an instance of this class; the
 * native code is called when the value of the setting changes (via onChange).
 *
 * <p>This bridge aggregates the stored setting and only notifies its observers when the logical
 * value changes.
 */
@NullMarked
@JNINamespace("supervised_user")
public class ContentFiltersObserverBridge {

    // Supervised User Content Filters Observer.
    private static final String TAG = "SUCFiltersObserver";

    // The observer to be notified of changes.
    private final ContentObserver mObserver;
    // Caches the logical value of the setting to avoid calling the native code with the same value.
    private boolean mIsEnabled;

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
                        boolean newEnabled = getValue(settingName);

                        if (mIsEnabled == newEnabled) {
                            Log.i(TAG, "setting=%s discarding %s", settingName, newEnabled);
                            return;
                        }
                        mIsEnabled = newEnabled;
                        ContentFiltersObserverBridgeJni.get()
                                .onChange(nativeContentFiltersObserverBridge, mIsEnabled);
                        Log.i(TAG, "setting=%s updating with %s", settingName, newEnabled);
                    }
                };
        ContextUtils.getApplicationContext()
                .getContentResolver()
                .registerContentObserver(
                        Settings.Secure.getUriFor(settingName),
                        /* notifyForDescendants= */ false,
                        mObserver);

        mIsEnabled = getValue(settingName);
        // Call the native first time to get the current value of the setting.
        ContentFiltersObserverBridgeJni.get()
                .onChange(nativeContentFiltersObserverBridge, mIsEnabled);
        Log.i(TAG, "setting=%s initial value %s", settingName, mIsEnabled);
    }

    @CalledByNative
    private void destroy() {
        ContextUtils.getApplicationContext()
                .getContentResolver()
                .unregisterContentObserver(mObserver);
    }

    @CalledByNative
    private boolean isEnabled() {
        return mIsEnabled;
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
