// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.device_signals;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper exposing Android OS device security signals to native code. */
@NullMarked
public final class DeviceSignalsUtils {
    private static final String TAG = "DeviceSignals";

    // Buckets of the Enterprise.DeviceSignals.StorageEncryption.Status histogram. Values 0-5 match
    // the DevicePolicyManager.ENCRYPTION_STATUS_* constants.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(DeviceSignalsStorageEncryptionStatus)
    @IntDef({
        StorageEncryptionStatusBucket.UNSUPPORTED,
        StorageEncryptionStatusBucket.INACTIVE,
        StorageEncryptionStatusBucket.ACTIVATING,
        StorageEncryptionStatusBucket.ACTIVE,
        StorageEncryptionStatusBucket.ACTIVE_DEFAULT_KEY,
        StorageEncryptionStatusBucket.ACTIVE_PER_USER,
        StorageEncryptionStatusBucket.UNRECOGNIZED
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface StorageEncryptionStatusBucket {
        int UNSUPPORTED = 0;
        int INACTIVE = 1;
        int ACTIVATING = 2;
        int ACTIVE = 3;
        int ACTIVE_DEFAULT_KEY = 4;
        int ACTIVE_PER_USER = 5;
        int UNRECOGNIZED = 6;
        int NUM_ENTRIES = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:DeviceSignalsStorageEncryptionStatus)

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
        RecordHistogram.recordEnumeratedHistogram(
                "Enterprise.DeviceSignals.StorageEncryption.Status",
                toStorageEncryptionStatusBucket(status),
                StorageEncryptionStatusBucket.NUM_ENTRIES);

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

    private static @StorageEncryptionStatusBucket int toStorageEncryptionStatusBucket(int status) {
        switch (status) {
            case DevicePolicyManager.ENCRYPTION_STATUS_UNSUPPORTED:
                return StorageEncryptionStatusBucket.UNSUPPORTED;
            case DevicePolicyManager.ENCRYPTION_STATUS_INACTIVE:
                return StorageEncryptionStatusBucket.INACTIVE;
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVATING:
                return StorageEncryptionStatusBucket.ACTIVATING;
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE:
                return StorageEncryptionStatusBucket.ACTIVE;
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_DEFAULT_KEY:
                return StorageEncryptionStatusBucket.ACTIVE_DEFAULT_KEY;
            case DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_PER_USER:
                return StorageEncryptionStatusBucket.ACTIVE_PER_USER;
            default:
                return StorageEncryptionStatusBucket.UNRECOGNIZED;
        }
    }
}
