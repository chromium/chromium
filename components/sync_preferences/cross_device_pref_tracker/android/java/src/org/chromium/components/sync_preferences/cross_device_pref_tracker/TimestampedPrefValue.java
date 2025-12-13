// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync_preferences.cross_device_pref_tracker;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Holds a Pref's `value`, the time of its last observed change,
 * and the Sync Cache GUID of the source device.
 *
 * @param <T> The type of the preference value.
 */
@NullMarked
@JNINamespace("sync_preferences")
public class TimestampedPrefValue<T> {
    private final T mValue;
    private final long mLastObservedChangeTimeMillis;
    private final String mDeviceSyncCacheGuid;

    protected TimestampedPrefValue(
            T value, long lastObservedChangeTimeMillis, String deviceSyncCacheGuid) {
        mValue = value;
        mLastObservedChangeTimeMillis = lastObservedChangeTimeMillis;
        mDeviceSyncCacheGuid = deviceSyncCacheGuid;
    }

    public T getValue() {
        return mValue;
    }

    public long getLastObservedChangeTimeMillis() {
        return mLastObservedChangeTimeMillis;
    }

    public String getDeviceSyncCacheGuid() {
        return mDeviceSyncCacheGuid;
    }

    @CalledByNative
    public static TimestampedPrefValue<Boolean> createBooleanPrefValue(
        boolean b, long time, @JniType("std::string") String deviceSyncCacheGuid) {
        return new TimestampedPrefValue<>(b, time, deviceSyncCacheGuid);
    }

    @CalledByNative
    public static TimestampedPrefValue<Double> createDoublePrefValue(
        double d, long time, @JniType("std::string") String deviceSyncCacheGuid) {
        return new TimestampedPrefValue<>(d, time, deviceSyncCacheGuid);
    }

    @CalledByNative
    public static TimestampedPrefValue<Integer> createIntegerPrefValue(
        int i, long time, @JniType("std::string") String deviceSyncCacheGuid) {
        return new TimestampedPrefValue<>(i, time, deviceSyncCacheGuid);
    }

    @CalledByNative
    public static TimestampedPrefValue<String> createStringPrefValue(
        @JniType("std::string") String s,
        long time,
        @JniType("std::string") String deviceSyncCacheGuid) {
        return new TimestampedPrefValue<>(s, time, deviceSyncCacheGuid);
    }
}
