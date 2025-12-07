// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync_preferences.cross_device_pref_tracker;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.OsType;

/**
 * A Java counterpart to the C++ CrossDevicePrefTracker. This class is created and owned by the C++
 * side.
 */
@NullMarked
@JNINamespace("sync_preferences")
public class CrossDevicePrefTracker {

    /** Observes cross-device preference changes. */
    @FunctionalInterface
    public interface CrossDevicePrefTrackerObserver {
        /**
         * Called when a preference value from a remote device has changed.
         *
         * @param prefName The name of the preference that changed.
         * @param timestampedPrefValue The new value & update timestamp ({@link
         *     TimestampedPrefValue}).
         * @param osType The OS type of the remote device.
         * @param formFactor The form factor of the remote device.
         */
        void onRemotePrefChanged(
                String prefName,
                TimestampedPrefValue timestampedPrefValue,
                @OsType int osType,
                @FormFactor int formFactor);
    }

    private long mNativePtr;
    private final ObserverList<CrossDevicePrefTrackerObserver> mObservers = new ObserverList<>();

    @CalledByNative
    private CrossDevicePrefTracker(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    public void addObserver(CrossDevicePrefTrackerObserver observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(CrossDevicePrefTrackerObserver observer) {
        mObservers.removeObserver(observer);
    }

    // TODO(crbug.com/442902926): Notify Java side of updates.
    // @CalledByNative
    // private void onRemotePrefChanged(
    //     String prefName,
    //     TimestampedPrefValue timestampedPrefValue,
    //     @OsType int osType,
    //     @FormFactor int formFactor) {
    //   for (CrossDevicePrefTrackerObserver observer : mObservers) {
    //       observer.onRemotePrefChanged(prefName, timestampedPrefValue, osType, formFactor);
    //   }
    // }

    /**
     * Returns all known values for the given {@param prefName} and device information filters.
     *
     * @param prefName The canonical name of the pref to query.
     * @param osType The {@link OsType} to use. If null, matches any OS type.
     * @param formFactor The {@link FormFactor} to use. If null, matches any form factor.
     * @param maxSyncRecencyMicroseconds A number of microseconds representing the maximum permitted
     *     difference between a device's last updated timestamp and the current time. If null,
     *     devices are included regardless of their last update time.
     */
    public TimestampedPrefValue[] getValues(
            String prefName,
            @Nullable @OsType Integer osType,
            @Nullable @FormFactor Integer formFactor,
            @Nullable Long maxSyncRecencyMicroseconds) {
        if (mNativePtr == 0) return new TimestampedPrefValue[0];
        return CrossDevicePrefTrackerJni.get()
                .getValues(mNativePtr, prefName, osType, formFactor, maxSyncRecencyMicroseconds);
    }

    /**
     * Returns all known values for the given {@param prefName} and device information filters.
     *
     * @param prefName The canonical name of the pref to query.
     * @param osType The {@link OsType} to use. If null, matches any OS type.
     * @param formFactor The {@link FormFactor} to use. If null, matches any form factor.
     * @param maxSyncRecencyMicroseconds A number of microseconds representing the maximum permitted
     *     difference between a device's last updated timestamp and the current time. If null,
     *     devices are included regardless of their last update time.
     */
    public @Nullable TimestampedPrefValue getMostRecentValue(
            String prefName,
            @Nullable @OsType Integer osType,
            @Nullable @FormFactor Integer formFactor,
            @Nullable Long maxSyncRecencyMicroseconds) {
        if (mNativePtr == 0) return null;
        return CrossDevicePrefTrackerJni.get()
                .getMostRecentValue(
                        mNativePtr, prefName, osType, formFactor, maxSyncRecencyMicroseconds);
    }

    @NativeMethods
    interface Natives {
        TimestampedPrefValue[] getValues(
                long nativeCrossDevicePrefTracker,
                String prefName,
                @JniType("std::optional<int>") @Nullable @OsType Integer osType,
                @JniType("std::optional<int>") @Nullable @FormFactor Integer formFactor,
                @JniType("std::optional<jlong>") @Nullable Long maxSyncRecencyMicroseconds);

        @Nullable TimestampedPrefValue getMostRecentValue(
                long nativeCrossDevicePrefTracker,
                String prefName,
                @JniType("std::optional<int>") @Nullable @OsType Integer osType,
                @JniType("std::optional<int>") @Nullable @FormFactor Integer formFactor,
                @JniType("std::optional<jlong>") @Nullable Long maxSyncRecencyMicroseconds);
    }
}
