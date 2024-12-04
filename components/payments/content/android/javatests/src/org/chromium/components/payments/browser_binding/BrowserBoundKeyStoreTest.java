// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import static org.junit.Assert.assertTrue;

import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;

import java.security.KeyPair;
import java.security.Signature;
import java.security.interfaces.ECPublicKey;

@RunWith(BaseJUnit4ClassRunner.class)
@MediumTest
@Batch(Batch.UNIT_TESTS)
@DisableIf.Build(message = "StrongBox is not available on x86", supported_abis_includes = "x86")
@DisableIf.Build(
        message = "StrongBox is not available on x86_64",
        supported_abis_includes = "x86_64")
public class BrowserBoundKeyStoreTest {

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testCreatesNewEcKey() {
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        byte[] credentialId = new byte[] {0x0a, 0x0b, 0x0c, 0x0d};

        BrowserBoundKey bbk =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(credentialId);

        KeyPair keyPair = bbk.getKeyPairForTesting();

        assertTrue(keyPair.getPublic() instanceof ECPublicKey);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void testFindsExistingKey() throws Exception {
        BrowserBoundKeyStore browserBoundKeyStore = BrowserBoundKeyStore.getInstance();
        byte[] credentialId = new byte[] {0x0a, 0x0b, 0x0c, 0x0d};
        byte[] clientData = new byte[] {0x01, 0x02, 0x03, 0x04};
        BrowserBoundKey bbk1 =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(credentialId);

        BrowserBoundKey bbk2 =
                browserBoundKeyStore.getOrCreateBrowserBoundKeyForCredentialId(credentialId);
        byte[] signature2 = bbk2.sign(clientData);

        // Verify the BBK2's signature using BBK1's public key.
        Signature signature = Signature.getInstance("SHA256withECDSA");
        signature.initVerify(bbk1.getKeyPairForTesting().getPublic());
        signature.update(clientData);
        assertTrue(signature.verify(signature2));
    }
}
