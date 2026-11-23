// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.pm.PackageManager;
import android.os.Build;
import android.security.keystore.KeyProperties;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.payments.PaymentFeatureList;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE)
public class BrowserBoundKeyStoreUnitTest {

    @Test
    public void testDeviceSupportsHardwareKeys_StrongBoxAvailable_ReturnsTrue() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE, true);
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE, true);

        assertTrue(BrowserBoundKeyStore.getDeviceSupportsHardwareKeys());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testDeviceSupportsHardwareKeys_OnlyTeeAvailable_ReturnsTrue() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE, false);
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE, true);

        assertTrue(BrowserBoundKeyStore.getDeviceSupportsHardwareKeys());
    }

    @Test
    public void testDeviceSupportsHardwareKeys_NoHardwareKeyStore_ReturnsFalse() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE, false);
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE, false);

        assertFalse(BrowserBoundKeyStore.getDeviceSupportsHardwareKeys());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testDeviceSupportsHardwareKeys_OnlyTeeAvailableBelowAndroidS_ReturnsFalse() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE, false);
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE, true);

        assertFalse(BrowserBoundKeyStore.getDeviceSupportsHardwareKeys());
    }

    @Test
    @DisableFeatures(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE)
    public void
            testDeviceSupportsHardwareKeys_StrongBoxAvailableWithTeeStorageDisabled_ReturnsTrue() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE, true);
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE, true);

        assertTrue(BrowserBoundKeyStore.getDeviceSupportsHardwareKeys());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    @DisableFeatures(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE)
    public void
            testDeviceSupportsHardwareKeys_OnlyTeeAvailableWithTeeStorageDisabled_ReturnsFalse() {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE, false);
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE, true);

        assertFalse(BrowserBoundKeyStore.getDeviceSupportsHardwareKeys());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testToCreateKeyResult_TrustedEnvironment() {
        assertEquals(
                BrowserBoundKeyStore.CreateKeyResult.TRUSTED_ENVIRONMENT,
                BrowserBoundKeyStore.toCreateKeyResult(
                        KeyProperties.SECURITY_LEVEL_TRUSTED_ENVIRONMENT));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testToCreateKeyResult_UnknownSecure() {
        assertEquals(
                BrowserBoundKeyStore.CreateKeyResult.UNKNOWN_SECURE,
                BrowserBoundKeyStore.toCreateKeyResult(
                        KeyProperties.SECURITY_LEVEL_UNKNOWN_SECURE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testToCreateKeyResult_Software() {
        assertEquals(
                BrowserBoundKeyStore.CreateKeyResult.SOFTWARE,
                BrowserBoundKeyStore.toCreateKeyResult(KeyProperties.SECURITY_LEVEL_SOFTWARE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testToCreateKeyResult_StrongBox() {
        assertEquals(
                BrowserBoundKeyStore.CreateKeyResult.STRONG_BOX,
                BrowserBoundKeyStore.toCreateKeyResult(KeyProperties.SECURITY_LEVEL_STRONGBOX));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testToCreateKeyResult_UnknownIsUnexpected() {
        assertEquals(
                BrowserBoundKeyStore.CreateKeyResult.UNEXPECTED_SECURITY_LEVEL,
                BrowserBoundKeyStore.toCreateKeyResult(KeyProperties.SECURITY_LEVEL_UNKNOWN));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testToCreateKeyResult_UndefinedValueIsUnexpected() {
        assertEquals(
                BrowserBoundKeyStore.CreateKeyResult.UNEXPECTED_SECURITY_LEVEL,
                BrowserBoundKeyStore.toCreateKeyResult(12345));
    }
}
