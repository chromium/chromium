// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.client_certificates;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.security.KeyPair;
import java.security.PrivateKey;

@NullMarked
public class BrowserKey {
    /** The logging tag for this class. */
    public static final String TAG = "TrustedAccess_BK";

    public static final String KEYSTORE_ALIAS_PREFIX = "ta_bk_";

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
