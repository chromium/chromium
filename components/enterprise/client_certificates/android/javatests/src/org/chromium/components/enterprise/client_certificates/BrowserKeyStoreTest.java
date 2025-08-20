// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.client_certificates;

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
import java.security.interfaces.ECPublicKey;
import java.util.Arrays;
import java.util.List;

@RunWith(BaseJUnit4ClassRunner.class)
@MediumTest
@Batch(Batch.UNIT_TESTS)
public class BrowserKeyStoreTest {
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
                            /* algorithmIdentifier= */ BrowserKey.COSE_ALGORITHM_ES256));

    private static PublicKeyCredentialParameters createCredentialParameters(
            int algorithmIdentifier) {
        PublicKeyCredentialParameters params = new PublicKeyCredentialParameters();
        params.type = PublicKeyCredentialType.PUBLIC_KEY;
        params.algorithmIdentifier = algorithmIdentifier;
        return params;
    }

    private static final byte[] BK_ID = new byte[] {0x0a, 0x0b, 0x0c, 0x0d};

    @After
    public void removeBk() {
        if (isStrongBoxAvailable()) {
            BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
            browserKeyStore.deleteBrowserKey(BK_ID);
        }
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testCreatesNewEcKey() {
        assumeTrue(isStrongBoxAvailable());
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();

        BrowserKey bk =
                browserKeyStore.getOrCreateBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);

        KeyPair keyPair = bk.getKeyPair();

        assertTrue(keyPair.getPublic() instanceof ECPublicKey);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testFindsExistingKey() throws Exception {
        assumeTrue(isStrongBoxAvailable());
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        byte[] clientData = new byte[] {0x01, 0x02, 0x03, 0x04};
        BrowserKey bk1 =
                browserKeyStore.getOrCreateBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);

        // Provide an empty list of algorithms expecting the existing key to be returned.
        BrowserKey bk2 =
                browserKeyStore.getOrCreateBrowserKeyForCredentialId(BK_ID, Arrays.asList());
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testDoesCreateKeyWithUnsupportedAlgorithm() throws Exception {
        assumeTrue(isStrongBoxAvailable());
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        BrowserKey bk =
                browserKeyStore.getOrCreateBrowserKeyForCredentialId(
                        BK_ID,
                        Arrays.asList(createCredentialParameters(/* algorithmIdentifier= */ 0)));

        assertNull(bk);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testDeletesAKey() throws Exception {
        assumeTrue(isStrongBoxAvailable());
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        BrowserKey bk =
                browserKeyStore.getOrCreateBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(bk);

        browserKeyStore.deleteBrowserKey(BK_ID);
        BrowserKey bkAfter =
                browserKeyStore.getOrCreateBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);

        assertNotNull(bkAfter);
        // The key pair should be different after deletion.
        assertFalse(Arrays.equals(bk.getPublicKeyAsSPKI(), bkAfter.getPublicKeyAsSPKI()));
    }
}
