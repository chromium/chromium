// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Log;

import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.Signature;
import java.security.SignatureException;

/**
 * A browser bound key pair for a matching passkey.
 *
 * <p>In SecurePaymentConfirmation get assertion requests, a browser bound key can be used to
 * provide an additional signature over the client data.
 */
public class BrowserBoundKey {

    /** The logging tag for this class. */
    private static final String TAG = "SpcBbKey";

    /** The signature algorithm to use for signing with the browser bound key. */
    private static final String SHA256_WITH_ECDSA = "SHA256withECDSA";

    private final KeyPair mKeyPair;

    BrowserBoundKey(KeyPair keyPair) {
        mKeyPair = keyPair;
    }

    @CalledByNative
    public byte[] sign(byte[] clientData) {
        Signature signature;
        try {
            signature = Signature.getInstance(SHA256_WITH_ECDSA);
        } catch (NoSuchAlgorithmException e) {
            // TODO: Eventually we will want to let Native code know that we can't do our job.
            Log.e(TAG, "Could not sign clientData for browser bound key support.", e);
            return null;
        }
        try {
            signature.initSign(mKeyPair.getPrivate());
            signature.update(clientData);
            return signature.sign();
        } catch (InvalidKeyException | SignatureException e) {
            // Neither of these should happen since we set up the algorithms in a fixed way and the
            // signature object's methods are called in the correct order: It is initialized then
            // updated then used to sign.
            throw new RuntimeException("Unexpected usage of Signature in BrowserBoundKey", e);
        }
    }

    private static final byte[] EMPTY_ARRAY = new byte[] {};

    /**
     * Returns the public key encoded as as COSE_Key including its algorithm type and parameters.
     *
     * <p>See credentialPublicKey in
     * https://www.w3.org/TR/webauthn-2/#sctn-attested-credential-data.
     */
    @CalledByNative
    @JniType("std::vector<uint8_t>")
    public byte[] getPublicKeyAsCoseKey() {
        // TODO(crbug.com/377278827): Encode the key as a COSE_Key.
        return EMPTY_ARRAY;
    }

    KeyPair getKeyPairForTesting() {
        return mKeyPair;
    }
}
