// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.password_manager;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@NullMarked
public class AndroidRequirements {
    private static @Nullable AndroidRequirements sInstance;
    private final boolean mHasMinGmsVersion;
    private final boolean mHasInternalBackend;

    @CalledByNative
    public static AndroidRequirements get() {
        if (sInstance != null) {
            return sInstance;
        }

        int gmsVersion = 0;
        try {
            // getGmsVersionCode() must be converted to int for comparison, because it can have
            // legacy values "3(...)" and those evaluate > "2025(...)".
            gmsVersion = Integer.parseInt(DeviceInfo.getGmsVersionCode());
        } catch (NumberFormatException e) {
            // Fall through.
        }
        int minVersion = DeviceInfo.isAutomotive() ? 241512000 : 240212000;
        sInstance =
                new AndroidRequirements(gmsVersion >= minVersion, BuildConfig.IS_CHROME_BRANDED);
        return sInstance;
    }

    /**
     * Checks whether the password manager can be used on Android. The criteria are:
     *
     * <p>1. GmsCore version with complete support to store and manage passwords.
     *
     * <p>2. Access to the internal codebase that interfaces with GmsCore (basically, having
     * checkout_src_internal=true).
     */
    @CalledByNative
    public boolean isPasswordManagerAvailable() {
        return mHasMinGmsVersion && mHasInternalBackend;
    }

    /** Weaker than isPasswordManagerAvailable(). You should use the former most of the time. */
    @CalledByNative
    public boolean hasMinGmsVersion() {
        return mHasMinGmsVersion;
    }

    /** Weaker than isPasswordManagerAvailable(). You should use the former most of the time. */
    @CalledByNative
    public boolean hasInternalBackend() {
        return mHasInternalBackend;
    }

    @CalledByNativeForTesting
    public static void setForTesting(AndroidRequirements instance) {
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = null);
    }

    @CalledByNativeForTesting
    @VisibleForTesting
    public AndroidRequirements(boolean hasMinGmsVersion, boolean hasInternalBackend) {
        mHasMinGmsVersion = hasMinGmsVersion;
        mHasInternalBackend = hasInternalBackend;
    }
}
