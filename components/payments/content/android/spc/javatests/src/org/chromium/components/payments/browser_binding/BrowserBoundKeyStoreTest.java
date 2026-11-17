// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.content.pm.PackageManager;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;

import java.security.KeyPair;
import java.security.Signature;
import java.security.interfaces.ECPublicKey;
import java.util.Arrays;
import java.util.List;

@RunWith(BaseJUnit4ClassRunner.class)
@MediumTest
@Batch(Batch.UNIT_TESTS)
public class BrowserBoundKeyStoreTest {
    private static Boolean sIsStrongBoxAvailable;

    private static boolean isStrongBoxAvailable() {
        if (sIsStrongBoxAvailable == null) {
            sIsStrongBoxAvailable =
                    InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getPackageManager()
                            .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);
        }
        return sIsStrongBoxAvailable;
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
        if (isStrongBoxAvailable()) {
            BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
            browserBoundKeyStore.deleteBrowserBoundKey(BBK_ID);
        }
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testCreatesNewEcKey() {
        assumeTrue(isStrongBoxAvailable());
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();

        BrowserBoundKey bbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID, ALLOWED_ALGORITHMS);

        KeyPair keyPair = bbk.getKeyPairForTesting();

        assertTrue(keyPair.getPublic() instanceof ECPublicKey);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testFindsExistingKey() throws Exception {
        assumeTrue(isStrongBoxAvailable());
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
    public void testDoesCreateKeyWithUnsupportedAlgorithm() throws Exception {
        assumeTrue(isStrongBoxAvailable());
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        BrowserBoundKey bbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(
                        BBK_ID,
                        Arrays.asList(createCredentialParameters(/* algorithmIdentifier= */ 0)));

        assertNull(bbk);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testDeletesAKey() throws Exception {
        assumeTrue(isStrongBoxAvailable());
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
    public void testDetectsStrongBoxSupport() {
        assertEquals(isStrongBoxAvailable(), BrowserBoundKeyStore.getDeviceSupportsHardwareKeys());
    }
}
