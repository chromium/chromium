// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.device_signals;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.ContextWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.device_signals.DeviceSignalsUtils.StorageEncryptionStatusBucket;

/** Tests for {@link DeviceSignalsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class DeviceSignalsUtilsTest {
    private static final String HISTOGRAM_NAME =
            "Enterprise.DeviceSignals.StorageEncryption.Status";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DevicePolicyManager mDevicePolicyManager;

    // Returned for Context.DEVICE_POLICY_SERVICE; null simulates a missing service.
    private DevicePolicyManager mDevicePolicyManagerToReturn;

    @Before
    public void setUp() {
        mDevicePolicyManagerToReturn = mDevicePolicyManager;
        ContextUtils.initApplicationContextForTests(
                new ContextWrapper(RuntimeEnvironment.getApplication()) {
                    @Override
                    public Object getSystemService(String name) {
                        if (Context.DEVICE_POLICY_SERVICE.equals(name)) {
                            return mDevicePolicyManagerToReturn;
                        }
                        return super.getSystemService(name);
                    }
                });
    }

    private void setStorageEncryptionStatus(int status) {
        when(mDevicePolicyManager.getStorageEncryptionStatus()).thenReturn(status);
    }

    // Checks both the returned signal value and the recorded histogram sample for `status`.
    private void assertDiskEncryption(
            int status,
            @SettingValue int expectedDiskEncryption,
            @StorageEncryptionStatusBucket int expectedBucket) {
        setStorageEncryptionStatus(status);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(HISTOGRAM_NAME, expectedBucket);

        assertEquals(expectedDiskEncryption, DeviceSignalsUtils.getDiskEncryption());
        watcher.assertExpected();
    }

    @Test
    public void testGetDiskEncryption_encrypted() {
        assertDiskEncryption(
                DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE,
                SettingValue.ENABLED,
                StorageEncryptionStatusBucket.ACTIVE);
        assertDiskEncryption(
                DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_DEFAULT_KEY,
                SettingValue.ENABLED,
                StorageEncryptionStatusBucket.ACTIVE_DEFAULT_KEY);
        assertDiskEncryption(
                DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_PER_USER,
                SettingValue.ENABLED,
                StorageEncryptionStatusBucket.ACTIVE_PER_USER);
    }

    @Test
    public void testGetDiskEncryption_notEncrypted() {
        assertDiskEncryption(
                DevicePolicyManager.ENCRYPTION_STATUS_INACTIVE,
                SettingValue.DISABLED,
                StorageEncryptionStatusBucket.INACTIVE);
        assertDiskEncryption(
                DevicePolicyManager.ENCRYPTION_STATUS_ACTIVATING,
                SettingValue.DISABLED,
                StorageEncryptionStatusBucket.ACTIVATING);
    }

    @Test
    public void testGetDiskEncryption_unsupported() {
        assertDiskEncryption(
                DevicePolicyManager.ENCRYPTION_STATUS_UNSUPPORTED,
                SettingValue.UNKNOWN,
                StorageEncryptionStatusBucket.UNSUPPORTED);
    }

    @Test
    public void testGetDiskEncryption_unrecognizedStatus() {
        assertDiskEncryption(99, SettingValue.UNKNOWN, StorageEncryptionStatusBucket.UNRECOGNIZED);
    }

    @Test
    public void testGetDiskEncryption_noDevicePolicyManager() {
        mDevicePolicyManagerToReturn = null;
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(HISTOGRAM_NAME).build();

        assertEquals(SettingValue.UNKNOWN, DeviceSignalsUtils.getDiskEncryption());
        watcher.assertExpected();
    }
}
