// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement.internal;

import androidx.annotation.CheckResult;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.ui.UiSwitches;

/**
 * Java side of the JNI bridge between TrackerImpl in Java
 * and C++. All method calls are delegated to the native C++ class.
 */
@JNINamespace("feature_engagement")
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
        TrackerImplJni.get().notifyEvent(mNativePtr, TrackerImpl.this, event);
    }

    @Override
    public boolean shouldTriggerHelpUI(String feature) {
        // Disable all IPH if the UI is in screenshot mode. For context, see crbug.com/964012.
        if (CommandLine.getInstance().hasSwitch(UiSwitches.ENABLE_SCREENSHOT_UI_MODE)) {
            return false;
        }

        assert mNativePtr != 0;
        return TrackerImplJni.get().shouldTriggerHelpUI(mNativePtr, TrackerImpl.this, feature);
    }

    @Override
    public TriggerDetails shouldTriggerHelpUIWithSnooze(String feature) {
        // Disable all IPH if the UI is in screenshot mode. For context, see crbug.com/964012.
        if (CommandLine.getInstance().hasSwitch(UiSwitches.ENABLE_SCREENSHOT_UI_MODE)) {
            return new TriggerDetails(false, false);
        }
        assert mNativePtr != 0;
        return TrackerImplJni.get()
                .shouldTriggerHelpUIWithSnooze(mNativePtr, TrackerImpl.this, feature);
    }

    @Override
    public boolean wouldTriggerHelpUI(String feature) {
        assert mNativePtr != 0;
        return TrackerImplJni.get().wouldTriggerHelpUI(mNativePtr, TrackerImpl.this, feature);
    }

    @Override
    public boolean hasEverTriggered(String feature, boolean fromWindow) {
        assert mNativePtr != 0;
        return TrackerImplJni.get()
                .hasEverTriggered(mNativePtr, TrackerImpl.this, feature, fromWindow);
    }

    @Override
    @TriggerState
    public int getTriggerState(String feature) {
        assert mNativePtr != 0;
        return TrackerImplJni.get().getTriggerState(mNativePtr, TrackerImpl.this, feature);
    }

    @Override
    public void dismissed(String feature) {
        assert mNativePtr != 0;
        TrackerImplJni.get().dismissed(mNativePtr, TrackerImpl.this, feature);
    }

    @Override
    public void dismissedWithSnooze(String feature, int snoozeAction) {
        assert mNativePtr != 0;
        TrackerImplJni.get()
                .dismissedWithSnooze(mNativePtr, TrackerImpl.this, feature, snoozeAction);
    }

    @Override
    @CheckResult
    @Nullable
    public DisplayLockHandle acquireDisplayLock() {
        assert mNativePtr != 0;
        return TrackerImplJni.get().acquireDisplayLock(mNativePtr, TrackerImpl.this);
    }

    @Override
    public void setPriorityNotification(String feature) {
        TrackerImplJni.get().setPriorityNotification(mNativePtr, TrackerImpl.this, feature);
    }

    @Override
    @Nullable
    public String getPendingPriorityNotification() {
        return TrackerImplJni.get().getPendingPriorityNotification(mNativePtr, TrackerImpl.this);
    }

    @Override
    public void registerPriorityNotificationHandler(
            String feature, Runnable priorityNotificationHandler) {
        TrackerImplJni.get()
                .registerPriorityNotificationHandler(
                        mNativePtr, TrackerImpl.this, feature, priorityNotificationHandler);
    }

    @Override
    public void unregisterPriorityNotificationHandler(String feature) {
        TrackerImplJni.get()
                .unregisterPriorityNotificationHandler(mNativePtr, TrackerImpl.this, feature);
    }

    @Override
    public boolean isInitialized() {
        assert mNativePtr != 0;
        return TrackerImplJni.get().isInitialized(mNativePtr, TrackerImpl.this);
    }

    @Override
    public void addOnInitializedCallback(Callback<Boolean> callback) {
        assert mNativePtr != 0;
        TrackerImplJni.get().addOnInitializedCallback(mNativePtr, TrackerImpl.this, callback);
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
        void notifyEvent(long nativeTrackerImplAndroid, TrackerImpl caller, String event);

        boolean shouldTriggerHelpUI(
                long nativeTrackerImplAndroid, TrackerImpl caller, String feature);

        TriggerDetails shouldTriggerHelpUIWithSnooze(
                long nativeTrackerImplAndroid, TrackerImpl caller, String feature);

        boolean wouldTriggerHelpUI(
                long nativeTrackerImplAndroid, TrackerImpl caller, String feature);

        boolean hasEverTriggered(
                long nativeTrackerImplAndroid,
                TrackerImpl caller,
                String feature,
                boolean fromWindow);

        int getTriggerState(long nativeTrackerImplAndroid, TrackerImpl caller, String feature);

        void dismissed(long nativeTrackerImplAndroid, TrackerImpl caller, String feature);

        void dismissedWithSnooze(
                long nativeTrackerImplAndroid,
                TrackerImpl caller,
                String feature,
                int snoozeAction);

        DisplayLockHandleAndroid acquireDisplayLock(
                long nativeTrackerImplAndroid, TrackerImpl caller);

        void setPriorityNotification(
                long nativeTrackerImplAndroid, TrackerImpl caller, String feature);

        String getPendingPriorityNotification(long nativeTrackerImplAndroid, TrackerImpl caller);

        void registerPriorityNotificationHandler(
                long nativeTrackerImplAndroid,
                TrackerImpl caller,
                String feature,
                Runnable priorityNotificationHandler);

        void unregisterPriorityNotificationHandler(
                long nativeTrackerImplAndroid, TrackerImpl caller, String feature);

        boolean isInitialized(long nativeTrackerImplAndroid, TrackerImpl caller);

        void addOnInitializedCallback(
                long nativeTrackerImplAndroid, TrackerImpl caller, Callback<Boolean> callback);

        void release(long nativeDisplayLockHandleAndroid);
    }
}
