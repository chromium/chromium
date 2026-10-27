// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.device_signals;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/** Helper exposing Android OS device security signals to native code. */
@NullMarked
public final class DeviceSignalsUtils {
    private static final String TAG = "DeviceSignals";

    private DeviceSignalsUtils() {}

    /** Returns whether the device is secured with a PIN, pattern or password. */
    @CalledByNative
    private static boolean isDeviceSecure() {
        KeyguardManager keyguardManager =
                assertNonNull(
                        ContextUtils.getApplicationContext()
                                .getSystemService(KeyguardManager.class));
        return keyguardManager.isDeviceSecure();
    }

    /**
     * Returns whether the device storage is encrypted, based on {@link
     * DevicePolicyManager#getStorageEncryptionStatus()}.
     */
    @CalledByNative
    @VisibleForTesting
    static @SettingValue int getDiskEncryption() {
        DevicePolicyManager devicePolicyManager =
                ContextUtils.getApplicationContext().getSystemService(DevicePolicyManager.class);
        if (devicePolicyManager == null) {
            return SettingValue.UNKNOWN;
        }

        int status = devicePolicyManager.getStorageEncryptionStatus();
        // ACTIVATING and ACTIVE_DEFAULT_KEY are only reported by full-disk encryption, which
        // exists up to Android 12.
        switch (status) {
            // ACTIVE_DEFAULT_KEY means the storage is encrypted, but with a key that is not tied
            // to the user's credential. Whether a credential is set is reported separately as
            // the screen lock signal.
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE:
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_DEFAULT_KEY:
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_PER_USER:
                return SettingValue.ENABLED;
            case DevicePolicyManager.ENCRYPTION_STATUS_INACTIVE:
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVATING:
                return SettingValue.DISABLED;
            case DevicePolicyManager.ENCRYPTION_STATUS_UNSUPPORTED:
                return SettingValue.UNKNOWN;
            default:
                Log.w(TAG, "Unknown storage encryption status: %d", status);
                return SettingValue.UNKNOWN;
        }
    }
}
