// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.prefs;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

/** PrefService provides read and write access to native PrefService. */
public class PrefService {
    private long mNativePrefServiceAndroid;

    @CalledByNative
    private static PrefService create(long nativePrefServiceAndroid) {
        return new PrefService(nativePrefServiceAndroid);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePrefServiceAndroid = 0;
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativePrefServiceAndroid;
    }

    @VisibleForTesting
    PrefService(long nativePrefServiceAndroid) {
        mNativePrefServiceAndroid = nativePrefServiceAndroid;
    }

    /** @param preference The name of the preference. */
    public void clearPref(@NonNull String preference) {
        PrefServiceJni.get().clearPref(mNativePrefServiceAndroid, preference);
    }

    /** @param preference The name of the preference. */
    public boolean hasPrefPath(@NonNull String preference) {
        return PrefServiceJni.get().hasPrefPath(mNativePrefServiceAndroid, preference);
    }

    /**
     * @param preference The name of the preference.
     * @return Whether the specified preference is enabled.
     */
    public boolean getBoolean(@NonNull String preference) {
        return PrefServiceJni.get().getBoolean(mNativePrefServiceAndroid, preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setBoolean(@NonNull String preference, boolean value) {
        PrefServiceJni.get().setBoolean(mNativePrefServiceAndroid, preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    public int getInteger(@NonNull String preference) {
        return PrefServiceJni.get().getInteger(mNativePrefServiceAndroid, preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setInteger(@NonNull String preference, int value) {
        PrefServiceJni.get().setInteger(mNativePrefServiceAndroid, preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    public double getDouble(@NonNull String preference) {
        return PrefServiceJni.get().getDouble(mNativePrefServiceAndroid, preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setDouble(@NonNull String preference, double value) {
        PrefServiceJni.get().setDouble(mNativePrefServiceAndroid, preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    public long getLong(@NonNull String preference) {
        return PrefServiceJni.get().getLong(mNativePrefServiceAndroid, preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setLong(@NonNull String preference, long value) {
        PrefServiceJni.get().setLong(mNativePrefServiceAndroid, preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    @NonNull
    public String getString(@NonNull String preference) {
        return PrefServiceJni.get().getString(mNativePrefServiceAndroid, preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setString(@NonNull String preference, @NonNull String value) {
        PrefServiceJni.get().setString(mNativePrefServiceAndroid, preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return Whether the specified preference is managed.
     */
    public boolean isManagedPreference(@NonNull String preference) {
        return PrefServiceJni.get().isManagedPreference(mNativePrefServiceAndroid, preference);
    }

    /**
     * @param preference The name of the preference
     * @return Whether the specified preference is currently using its default value
     * and has not been set by any higher-priority source (even with the same value).
     */
    public boolean isDefaultValuePreference(@NonNull String preference) {
        return PrefServiceJni.get().isDefaultValuePreference(mNativePrefServiceAndroid, preference);
    }

    @NativeMethods
    interface Natives {
        void clearPref(long nativePrefServiceAndroid, String preference);

        boolean hasPrefPath(long nativePrefServiceAndroid, String preference);

        boolean getBoolean(long nativePrefServiceAndroid, String preference);

        void setBoolean(long nativePrefServiceAndroid, String preference, boolean value);

        int getInteger(long nativePrefServiceAndroid, String preference);

        void setInteger(long nativePrefServiceAndroid, String preference, int value);

        double getDouble(long nativePrefServiceAndroid, String preference);

        void setDouble(long nativePrefServiceAndroid, String preference, double value);

        long getLong(long nativePrefServiceAndroid, String preference);

        void setLong(long nativePrefServiceAndroid, String preference, long value);

        String getString(long nativePrefServiceAndroid, String preference);

        void setString(long nativePrefServiceAndroid, String preference, String value);

        boolean isManagedPreference(long nativePrefServiceAndroid, String preference);

        boolean isDefaultValuePreference(long nativePrefServiceAndroid, String preference);
    }
}
