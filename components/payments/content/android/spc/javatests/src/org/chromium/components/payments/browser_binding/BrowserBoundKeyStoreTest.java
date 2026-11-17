// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.content.pm.PackageManager;
import android.os.Build;
import android.security.keystore.KeyInfo;
import android.security.keystore.KeyProperties;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.components.payments.PaymentFeatureList;

import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.Signature;
import java.security.interfaces.ECPublicKey;
import java.util.Arrays;
import java.util.List;

@RunWith(BaseJUnit4ClassRunner.class)
@MediumTest
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE)
public class BrowserBoundKeyStoreTest {
    private static boolean isStrongBoxAvailable() {
        return InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);
    }

    private static boolean isHardwareKeyStoreAvailable() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getPackageManager()
                        .hasSystemFeature(PackageManager.FEATURE_HARDWARE_KEYSTORE);
    }

    private static boolean isHardwareBackedKeyAvailable() {
        return isStrongBoxAvailable() || isHardwareKeyStoreAvailable();
    }

    private static final List<PublicKeyCredentialParameters> ALLOWED_ALGORITHMS =
            Arrays.asList(
                    createCredentialParameters(
                            /* algorithmIdentifier= */ BrowserBoundKey.COSE_ALGORITHM_ES256));

    private static PublicKeyCredentialParameters createCredentialParameters(
            int algorithmIdentifier) {
        PublicKeyCredentialParameters params = new PublicKeyCredentialParameters();
        params.type = PublicKeyCredentialType.PUBLIC_KEY;
        params.algorithmIdentifier = algorithmIdentifier;
        return params;
    }

    private static final byte[] BBK_ID = new byte[] {0x0a, 0x0b, 0x0c, 0x0d};

    @After
    public void removeBbk() {
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        browserBoundKeyStore.deleteBrowserBoundKey(BBK_ID);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testCreatesNewEcKey() throws Exception {
        assumeTrue(isHardwareBackedKeyAvailable());
        BrowserBoundKeyStore.setAllowSoftwareKeysForTesting(true);
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();

        BrowserBoundKey bbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);

        assertNotNull(bbk);
        KeyPair keyPair = bbk.getKeyPairForTesting();
        assertTrue(keyPair.getPublic() instanceof ECPublicKey);

        KeyFactory keyFactory =
                KeyFactory.getInstance(
                        keyPair.getPrivate().getAlgorithm(),
                        BrowserBoundKeyStore.ANDROID_KEY_STORE);
        KeyInfo keyInfo = keyFactory.getKeySpec(keyPair.getPrivate(), KeyInfo.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (isStrongBoxAvailable()) {
                assertEquals(KeyProperties.SECURITY_LEVEL_STRONGBOX, keyInfo.getSecurityLevel());
            } else {
                // Non-StrongBox bots in the test fleet are emulators with software-backed keystores
                // (allowed via setAllowSoftwareKeysForTesting(true)).
                assertEquals(KeyProperties.SECURITY_LEVEL_SOFTWARE, keyInfo.getSecurityLevel());
            }
        } else {
            assertTrue(keyInfo.isInsideSecureHardware());
        }
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.S)
    public void testDoesNotCreateSoftwareKeyWhenTeeKeystoreIsNotHardwareBacked() {
        assumeFalse(isStrongBoxAvailable());
        assumeTrue(isHardwareKeyStoreAvailable());
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();

        BrowserBoundKey bbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);

        assertNull(bbk);
        // Allow software keys so that a leftover software key would be found.
        BrowserBoundKeyStore.setAllowSoftwareKeysForTesting(true);
        assertNull(
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, Arrays.asList()));
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testDoesNotCreateKeyWithUnsupportedAlgorithm() throws Exception {
        assumeTrue(isHardwareBackedKeyAvailable());
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        BrowserBoundKey bbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID,
                        Arrays.asList(createCredentialParameters(/* algorithmIdentifier= */ 0)));

        assertNull(bbk);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testFindsExistingKey() throws Exception {
        assumeTrue(isHardwareBackedKeyAvailable());
        BrowserBoundKeyStore.setAllowSoftwareKeysForTesting(true);
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        byte[] clientData = new byte[] {0x01, 0x02, 0x03, 0x04};
        BrowserBoundKey bbk1 =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);

        // Provide an empty list of algorithms expecting the existing key to be returned.
        BrowserBoundKey bbk2 =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, Arrays.asList());
        byte[] signature2 = bbk2.sign(clientData);

        // Verify the BBK2's signature using BBK1's public key.
        Signature signature = Signature.getInstance("SHA256withECDSA");
        signature.initVerify(bbk1.getKeyPairForTesting().getPublic());
        signature.update(clientData);
        assertTrue(signature.verify(signature2));
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testIgnoresAndRestoresExistingTeeKeyWhenTeeStorageToggled() {
        assumeFalse(isStrongBoxAvailable());
        assumeTrue(isHardwareKeyStoreAvailable());
        BrowserBoundKeyStore.setAllowSoftwareKeysForTesting(true);
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();

        FeatureOverrides.disable(
                PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE);
        assertNull(
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS));

        FeatureOverrides.enable(
                PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE);
        BrowserBoundKey teeBbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(teeBbk);

        FeatureOverrides.disable(
                PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE);
        assertNull(
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, Arrays.asList()));
        assertNull(
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS));

        FeatureOverrides.enable(
                PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE);
        BrowserBoundKey restoredBbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, Arrays.asList());

        assertNotNull(restoredBbk);
        assertArrayEquals(teeBbk.getPublicKeyAsCoseKey(), restoredBbk.getPublicKeyAsCoseKey());
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testDeletesAKey() throws Exception {
        assumeTrue(isHardwareBackedKeyAvailable());
        BrowserBoundKeyStore.setAllowSoftwareKeysForTesting(true);
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        BrowserBoundKey bbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(bbk);

        browserBoundKeyStore.deleteBrowserBoundKey(BBK_ID);
        BrowserBoundKey bbkAfter =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);

        assertNotNull(bbkAfter);
        // Assert a new bbk was created by comparing the public keys.
        assertFalse(Arrays.equals(bbk.getPublicKeyAsCoseKey(), bbkAfter.getPublicKeyAsCoseKey()));
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testDeletesExistingTeeKeyWhenTeeStorageDisabled() {
        assumeFalse(isStrongBoxAvailable());
        assumeTrue(isHardwareKeyStoreAvailable());
        BrowserBoundKeyStore.setAllowSoftwareKeysForTesting(true);
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        BrowserBoundKey teeBbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(teeBbk);
        FeatureOverrides.disable(
                PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE);

        browserBoundKeyStore.deleteBrowserBoundKey(BBK_ID);

        FeatureOverrides.enable(
                PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION_BBKS_ALLOW_TEE_STORAGE);
        assertNull(
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, Arrays.asList()));
    }
}
