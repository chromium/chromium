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

/** Tests for {@link DeviceSignalsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class DeviceSignalsUtilsTest {
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

    @Test
    public void testGetDiskEncryption_encrypted() {
        setStorageEncryptionStatus(DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE);
        assertEquals(SettingValue.ENABLED, DeviceSignalsUtils.getDiskEncryption());

        setStorageEncryptionStatus(DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_DEFAULT_KEY);
        assertEquals(SettingValue.ENABLED, DeviceSignalsUtils.getDiskEncryption());

        setStorageEncryptionStatus(DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_PER_USER);
        assertEquals(SettingValue.ENABLED, DeviceSignalsUtils.getDiskEncryption());
    }

    @Test
    public void testGetDiskEncryption_notEncrypted() {
        setStorageEncryptionStatus(DevicePolicyManager.ENCRYPTION_STATUS_INACTIVE);
        assertEquals(SettingValue.DISABLED, DeviceSignalsUtils.getDiskEncryption());

        setStorageEncryptionStatus(DevicePolicyManager.ENCRYPTION_STATUS_ACTIVATING);
        assertEquals(SettingValue.DISABLED, DeviceSignalsUtils.getDiskEncryption());
    }

    @Test
    public void testGetDiskEncryption_unsupported() {
        setStorageEncryptionStatus(DevicePolicyManager.ENCRYPTION_STATUS_UNSUPPORTED);
        assertEquals(SettingValue.UNKNOWN, DeviceSignalsUtils.getDiskEncryption());
    }

    @Test
    public void testGetDiskEncryption_unrecognizedStatus() {
        setStorageEncryptionStatus(99);
        assertEquals(SettingValue.UNKNOWN, DeviceSignalsUtils.getDiskEncryption());
    }

    @Test
    public void testGetDiskEncryption_noDevicePolicyManager() {
        mDevicePolicyManagerToReturn = null;
        assertEquals(SettingValue.UNKNOWN, DeviceSignalsUtils.getDiskEncryption());
    }
}
