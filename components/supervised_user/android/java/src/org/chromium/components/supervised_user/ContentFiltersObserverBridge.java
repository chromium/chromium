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
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
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
public class ContentFiltersObserverBridge {

    // Supervised User Content Filters Observer.
    private static final String TAG = "SUCFiltersObserver";

    // Task runner for off-UI-thread ContentObserver registration and Settings.Secure pre-warming.
    private static final SequencedTaskRunner sBackgroundTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE_MAY_BLOCK);

    // Notified asynchronously from secure settings.
    private final ContentObserver mObserver;
    // Triggered when application state changes, to read the secure settings synchronously.
    private final ApplicationStatus.ApplicationStateListener mStateListener;
    // Caches the logical value of the setting to avoid calling the native code with the same value.
    private @Nullable Boolean mIsEnabled;
    private boolean mDestroyed;

    /**
     * Pre-warms Android's {@link Settings.Secure} in-process cache for the supervised user content
     * filter settings on a background thread so the initial synchronous read in the constructor
     * does not block the UI thread on Binder IPC.
     */
    public static void warmUp() {
        sBackgroundTaskRunner.execute(
                () -> {
                    getValue(SupervisedUserConstants.BROWSER_CONTENT_FILTERS_SETTING_NAME);
                    getValue(SupervisedUserConstants.SEARCH_CONTENT_FILTERS_SETTING_NAME);
                });
    }

    /**
     * @param nativeContentFiltersObserverBridge The native bridge.
     * @param settingName The name of the setting to observe.
     */
    @CalledByNative
    ContentFiltersObserverBridge(long nativeContentFiltersObserverBridge, String settingName) {
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

        sBackgroundTaskRunner.execute(
                () -> {
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .registerContentObserver(
                                    Settings.Secure.getUriFor(settingName),
                                    /* notifyForDescendants= */ false,
                                    mObserver);
                    ThreadUtils.postOnUiThread(
                            () ->
                                    updateFromSecureSettings(
                                            nativeContentFiltersObserverBridge, settingName));
                });
        ApplicationStatus.registerApplicationStateListener(mStateListener);
        updateFromSecureSettings(nativeContentFiltersObserverBridge, settingName);
    }

    /**
     * Updates internal state synchronously from secure settings. If the value of setting changed or
     * upon the first read, then the native code is notified.
     */
    private void updateFromSecureSettings(
            long nativeContentFiltersObserverBridge, String settingName) {
        if (mDestroyed) {
            return;
        }
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
    void destroy() {
        mDestroyed = true;
        sBackgroundTaskRunner.execute(
                () ->
                        ContextUtils.getApplicationContext()
                                .getContentResolver()
                                .unregisterContentObserver(mObserver));
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
