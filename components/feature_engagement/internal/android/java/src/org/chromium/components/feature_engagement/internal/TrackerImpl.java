// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement.internal;

import androidx.annotation.CheckResult;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.ui.UiSwitches;

/**
 * Java side of the JNI bridge between TrackerImpl in Java
 * and C++. All method calls are delegated to the native C++ class.
 */
@JNINamespace("feature_engagement")
@NullMarked
public class TrackerImpl implements Tracker {
    /**
     * A JNI-wrapper for the native DisplayLockHandle.
     * The C++ counterpart is DisplayLockHandleAndroid.
     */
    static class DisplayLockHandleAndroid implements DisplayLockHandle {
        @CalledByNative("DisplayLockHandleAndroid")
        private static DisplayLockHandleAndroid create(long nativePtr) {
            return new DisplayLockHandleAndroid(nativePtr);
        }

        long mNativePtr;

        private DisplayLockHandleAndroid(long nativePtr) {
            mNativePtr = nativePtr;
        }

        @CalledByNative("DisplayLockHandleAndroid")
        private void clearNativePtr() {
            mNativePtr = 0;
        }

        @Override
        public void release() {
            assert mNativePtr != 0;
            TrackerImplJni.get().release(mNativePtr);
            assert mNativePtr == 0;
        }
    }

    /** The pointer to the feature_engagement::TrackerImplAndroid JNI bridge. */
    private long mNativePtr;

    @CalledByNative
    private static TrackerImpl create(long nativePtr) {
        return new TrackerImpl(nativePtr);
    }

    private TrackerImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void notifyEvent(String event) {
        assert mNativePtr != 0;
        TrackerImplJni.get().notifyEvent(mNativePtr, event);
    }

    @Override
    public boolean shouldTriggerHelpUi(String feature) {
        // Disable all IPH if the UI is in screenshot mode. For context, see crbug.com/964012.
        if (CommandLine.getInstance().hasSwitch(UiSwitches.ENABLE_SCREENSHOT_UI_MODE)) {
            return false;
        }

        assert mNativePtr != 0;
        return TrackerImplJni.get().shouldTriggerHelpUi(mNativePtr, feature);
    }

    @Override
    public TriggerDetails shouldTriggerHelpUiWithSnooze(String feature) {
        // Disable all IPH if the UI is in screenshot mode. For context, see crbug.com/964012.
        if (CommandLine.getInstance().hasSwitch(UiSwitches.ENABLE_SCREENSHOT_UI_MODE)) {
            return new TriggerDetails(false, false);
        }
        assert mNativePtr != 0;
        return TrackerImplJni.get().shouldTriggerHelpUiWithSnooze(mNativePtr, feature);
    }

    @Override
    public boolean wouldTriggerHelpUi(String feature) {
        assert mNativePtr != 0;
        return TrackerImplJni.get().wouldTriggerHelpUi(mNativePtr, feature);
    }

    @Override
    public boolean hasEverTriggered(String feature, boolean fromWindow) {
        assert mNativePtr != 0;
        return TrackerImplJni.get().hasEverTriggered(mNativePtr, feature, fromWindow);
    }

    @Override
    @TriggerState
    public int getTriggerState(String feature) {
        assert mNativePtr != 0;
        return TrackerImplJni.get().getTriggerState(mNativePtr, feature);
    }

    @Override
    public void dismissed(String feature) {
        assert mNativePtr != 0;
        TrackerImplJni.get().dismissed(mNativePtr, feature);
    }

    @Override
    public void dismissedWithSnooze(String feature, int snoozeAction) {
        assert mNativePtr != 0;
        TrackerImplJni.get().dismissedWithSnooze(mNativePtr, feature, snoozeAction);
    }

    @Override
    @CheckResult
    public @Nullable DisplayLockHandle acquireDisplayLock() {
        assert mNativePtr != 0;
        return TrackerImplJni.get().acquireDisplayLock(mNativePtr);
    }

    @Override
    public void setPriorityNotification(String feature) {
        TrackerImplJni.get().setPriorityNotification(mNativePtr, feature);
    }

    @Override
    public @Nullable String getPendingPriorityNotification() {
        return TrackerImplJni.get().getPendingPriorityNotification(mNativePtr);
    }

    @Override
    public void registerPriorityNotificationHandler(
            String feature, Runnable priorityNotificationHandler) {
        TrackerImplJni.get()
                .registerPriorityNotificationHandler(
                        mNativePtr, feature, priorityNotificationHandler);
    }

    @Override
    public void unregisterPriorityNotificationHandler(String feature) {
        TrackerImplJni.get().unregisterPriorityNotificationHandler(mNativePtr, feature);
    }

    @Override
    public boolean isInitialized() {
        assert mNativePtr != 0;
        return TrackerImplJni.get().isInitialized(mNativePtr);
    }

    @Override
    public void addOnInitializedCallback(Callback<Boolean> callback) {
        assert mNativePtr != 0;
        TrackerImplJni.get().addOnInitializedCallback(mNativePtr, callback);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativePtr != 0;
        return mNativePtr;
    }

    @CalledByNative
    private static TriggerDetails createTriggerDetails(
            boolean shouldTriggerIph, boolean shouldShowSnooze) {
        return new TriggerDetails(shouldTriggerIph, shouldShowSnooze);
    }

    @NativeMethods
    interface Natives {
        void notifyEvent(long nativeTrackerImplAndroid, String event);

        boolean shouldTriggerHelpUi(long nativeTrackerImplAndroid, String feature);

        TriggerDetails shouldTriggerHelpUiWithSnooze(long nativeTrackerImplAndroid, String feature);

        boolean wouldTriggerHelpUi(long nativeTrackerImplAndroid, String feature);

        boolean hasEverTriggered(long nativeTrackerImplAndroid, String feature, boolean fromWindow);

        int getTriggerState(long nativeTrackerImplAndroid, String feature);

        void dismissed(long nativeTrackerImplAndroid, String feature);

        void dismissedWithSnooze(long nativeTrackerImplAndroid, String feature, int snoozeAction);

        DisplayLockHandleAndroid acquireDisplayLock(long nativeTrackerImplAndroid);

        void setPriorityNotification(long nativeTrackerImplAndroid, String feature);

        String getPendingPriorityNotification(long nativeTrackerImplAndroid);

        void registerPriorityNotificationHandler(
                long nativeTrackerImplAndroid,
                String feature,
                Runnable priorityNotificationHandler);

        void unregisterPriorityNotificationHandler(long nativeTrackerImplAndroid, String feature);

        boolean isInitialized(long nativeTrackerImplAndroid);

        void addOnInitializedCallback(long nativeTrackerImplAndroid, Callback<Boolean> callback);

        void release(long nativeDisplayLockHandleAndroid);
    }
}
