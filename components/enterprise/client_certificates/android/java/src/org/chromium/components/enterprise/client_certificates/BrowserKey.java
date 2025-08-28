// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.client_certificates;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.Signature;
import java.security.SignatureException;

@NullMarked
public class BrowserKey {
    /** The logging tag for this class. */
    public static final String TAG = "TrustedAccess_BK";

    public static final String KEYSTORE_ALIAS_PREFIX = "ta_bk_";

    /** The signature algorithm to use for signing with the browser key. */
    private static final String SHA256_WITH_ECDSA = "SHA256withECDSA";

    /**
     * The cose algorithm identifier for SHA256 with ECDSA.
     *
     * <p>See the <a href="https://www.iana.org/assignments/cose/cose.xhtml#algorithms">COSE
     * Algorithms registry</a>
     */
    public static final int COSE_ALGORITHM_ES256 = -7;

    private final byte[] mIdentifier;
    private final KeyPair mKeyPair;

    /**
     * Creates a browser key given the identifier and keyPair.
     *
     * @param identifier The identifier of the browser key.
     * @param keyPair The KeyPair of this browser key obtained from a KeyStore.
     */
    BrowserKey(byte[] identifier, KeyPair keyPair) {
        mIdentifier = identifier;
        mKeyPair = keyPair;
    }

    KeyPair getKeyPair() {
        return mKeyPair;
    }

    @CalledByNative
    public byte @Nullable [] sign(byte[] data) {
        Signature signature;
        try {
            // TODO(crbug.com/432304139): Support various signature algorithm.
            signature = Signature.getInstance(SHA256_WITH_ECDSA);
        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG, "Could not sign data for browser key support.", e);
            return null;
        }
        try {
            signature.initSign(mKeyPair.getPrivate());
            signature.update(data);
            return signature.sign();
        } catch (InvalidKeyException | SignatureException e) {
            // Neither of these should happen since we set up the algorithms in a fixed way and the
            // signature object's methods are called in the correct order: It is initialized then
            // updated then used to sign.
            throw new RuntimeException("Unexpected usage of Signature in BrowserKey.", e);
        }
    }

    @CalledByNative
    @JniType("std::vector<uint8_t>")
    public byte[] getIdentifier() {
        return mIdentifier;
    }

    @CalledByNative
    PrivateKey getPrivateKey() {
        return mKeyPair.getPrivate();
    }

    @CalledByNative
    @JniType("std::vector<uint8_t>")
    byte @Nullable [] getPublicKeyAsSPKI() {
        return mKeyPair.getPublic().getEncoded();
    }
}
