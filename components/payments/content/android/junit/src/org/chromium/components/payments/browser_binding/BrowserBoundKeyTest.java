// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import static org.junit.Assert.assertTrue;

import android.security.keystore.KeyProperties;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.Signature;
import java.security.spec.ECGenParameterSpec;

@RunWith(BaseRobolectricTestRunner.class)
public class BrowserBoundKeyTest {

    // TODO(crbug.com/377278827): Create a test with an imported known key and literal signature
    // comparison.
    // TODO(crbug.com/377278827): Test with more algorithms than only ES256.
    @Test
    public void testSignWithEs256() throws Exception {
        KeyPairGenerator keyPairGenerator =
                KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC);
        // Use any parameter spec with 256bit size for this test.
        keyPairGenerator.initialize(new ECGenParameterSpec("prime256v1"));
        KeyPair keyPair = keyPairGenerator.generateKeyPair();
        BrowserBoundKey browserBoundKey = new BrowserBoundKey(keyPair);
        byte[] clientData = {0x01, 0x02, 0x03, 0x04};

        byte[] actualSignature = browserBoundKey.sign(clientData);

        Signature signature = Signature.getInstance("SHA256withECDSA");
        signature.initVerify(keyPair.getPublic());
        signature.update(clientData);
        assertTrue(signature.verify(actualSignature));
    }
}
