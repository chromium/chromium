// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.client_certificates;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.security.KeyPair;
import java.security.Signature;
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
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        browserKeyStore.deleteBrowserKey(BK_ID);
    }

    private void setSdkInt(int sdkInt) throws Exception {
        Field sdkIntField = Build.VERSION.class.getField("SDK_INT");
        sdkIntField.setAccessible(true);
        Field modifiersField = Field.class.getDeclaredField("modifiers");
        modifiersField.setAccessible(true);
        modifiersField.setInt(sdkIntField, sdkIntField.getModifiers() & ~Modifier.FINAL);
        sdkIntField.set(null, sdkInt);
    }

    @Test
    public void testDoesNotCreateKeyOnOldApi() throws Exception {
        int originalSdkInt = Build.VERSION.SDK_INT;
        try {
            setSdkInt(Build.VERSION_CODES.LOLLIPOP); // API 21, M is 23.

            BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
            BrowserKey bk =
                    browserKeyStore.createBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);
            assertNull(bk);
        } finally {
            setSdkInt(originalSdkInt);
        }
    }

    @Test
    public void testCreatesNewEcKey() {
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();

        BrowserKey bk = browserKeyStore.createBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);

        KeyPair keyPair = bk.getKeyPair();

        assertTrue(keyPair.getPublic() instanceof ECPublicKey);
    }

    @Test
    public void testDoesNotCreateKeyWithUnsupportedAlgorithm() throws Exception {
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        BrowserKey bk =
                browserKeyStore.createBrowserKeyForCredentialId(
                        BK_ID,
                        Arrays.asList(createCredentialParameters(/* algorithmIdentifier= */ 0)));

        assertNull(bk);
    }

    @Test
    public void testCreateReplacesExistingKey() throws Exception {
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        BrowserKey bk1 = browserKeyStore.createBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(bk1);

        BrowserKey bk2 = browserKeyStore.createBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(bk2);

        assertFalse(Arrays.equals(bk1.getPublicKeyAsSPKI(), bk2.getPublicKeyAsSPKI()));
        BrowserKey loadedBk = browserKeyStore.getBrowserKeyForCredentialId(BK_ID);
        assertNotNull(loadedBk);
        assertArrayEquals(bk2.getPublicKeyAsSPKI(), loadedBk.getPublicKeyAsSPKI());
    }

    @Test
    public void testGetDoesNotCreateMissingKey() {
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        browserKeyStore.deleteBrowserKey(BK_ID);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                BrowserKeyStore.KEY_LOAD_RESULT_HISTOGRAM,
                                BrowserKeyStore.KeyLoadResult.NOT_FOUND,
                                2)
                        .build();

        assertNull(browserKeyStore.getBrowserKeyForCredentialId(BK_ID));
        // The lookup must not have created a key as a side effect.
        assertNull(browserKeyStore.getBrowserKeyForCredentialId(BK_ID));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetFindsExistingKey() throws Exception {
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        byte[] clientData = new byte[] {0x01, 0x02, 0x03, 0x04};
        BrowserKey createdBk =
                browserKeyStore.createBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(createdBk);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BrowserKeyStore.KEY_LOAD_RESULT_HISTOGRAM,
                        BrowserKeyStore.KeyLoadResult.FOUND);

        BrowserKey loadedBk = browserKeyStore.getBrowserKeyForCredentialId(BK_ID);
        assertNotNull(loadedBk);
        histogramWatcher.assertExpected();

        // Verify the loaded key's signature using the created key's public key.
        byte[] signature = loadedBk.sign(clientData);
        Signature verifier = Signature.getInstance("SHA256withECDSA");
        verifier.initVerify(createdBk.getKeyPair().getPublic());
        verifier.update(clientData);
        assertTrue(verifier.verify(signature));
    }

    @Test
    public void testGetAfterDeleteReturnsNull() {
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        assertNotNull(browserKeyStore.createBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS));

        browserKeyStore.deleteBrowserKey(BK_ID);

        assertNull(browserKeyStore.getBrowserKeyForCredentialId(BK_ID));
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testReportsCorrectSecurityLevel() {
        BrowserKeyStore browserKeyStore = BrowserKeyStore.getInstance();
        BrowserKey bk = browserKeyStore.createBrowserKeyForCredentialId(BK_ID, ALLOWED_ALGORITHMS);
        assertNotNull(bk);

        if (isStrongBoxAvailable()) {
            assertEquals(BrowserKey.SecurityLevel.STRONGBOX, bk.getSecurityLevel());
        } else {
            // On devices without StrongBox, the key could be in a TEE or software-backed.
            // Emulators will likely be software.
            int securityLevel = bk.getSecurityLevel();
            assertTrue(
                    "Security level was: " + securityLevel,
                    securityLevel == BrowserKey.SecurityLevel.TRUSTED_ENVIRONMENT
                            || securityLevel == BrowserKey.SecurityLevel.OS_SOFTWARE);
        }
    }
}
