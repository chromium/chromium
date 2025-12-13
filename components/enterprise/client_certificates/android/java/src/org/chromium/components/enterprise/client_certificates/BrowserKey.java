// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.client_certificates;

import android.os.Build;
import android.security.keystore.KeyInfo;
import android.security.keystore.KeyProperties;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.security.InvalidKeyException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.PrivateKey;
import java.security.Signature;
import java.security.SignatureException;
import java.security.spec.InvalidKeySpecException;

@NullMarked
public class BrowserKey {
    /** The logging tag for this class. */
    public static final String TAG = "TrustedAccess_BK";

    /** The security level of the browser key. */
    /**
     * Has to match the SecurityLevel enum in
     * client_certificates/android/browser_binding/browser_key.h
     */
    @IntDef({
        SecurityLevel.OS_SOFTWARE,
        SecurityLevel.TRUSTED_ENVIRONMENT,
        SecurityLevel.STRONGBOX,
        SecurityLevel.UNKNOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SecurityLevel {
        /** The key is not secured by hardware. */
        int OS_SOFTWARE = 0;

        /** The key is secured by a Trusted Execution Environment (TEE). */
        int TRUSTED_ENVIRONMENT = 1;

        /** The key is secured by a StrongBox. */
        int STRONGBOX = 2;

        /** The key security is unknown. */
        int UNKNOWN = 3;
    }

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

    @CalledByNative
    public @SecurityLevel int getSecurityLevel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return SecurityLevel.OS_SOFTWARE;
        }

        try {
            KeyFactory factory =
                    KeyFactory.getInstance(
                            mKeyPair.getPrivate().getAlgorithm(),
                            BrowserKeyStore.ANDROID_KEY_STORE);
            KeyInfo keyInfo = factory.getKeySpec(mKeyPair.getPrivate(), KeyInfo.class);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                switch (keyInfo.getSecurityLevel()) {
                    case KeyProperties.SECURITY_LEVEL_STRONGBOX:
                        return SecurityLevel.STRONGBOX;
                    case KeyProperties.SECURITY_LEVEL_TRUSTED_ENVIRONMENT:
                    case KeyProperties.SECURITY_LEVEL_UNKNOWN_SECURE:
                        return SecurityLevel.TRUSTED_ENVIRONMENT;
                    case KeyProperties.SECURITY_LEVEL_UNKNOWN:
                        return SecurityLevel.UNKNOWN;
                    default:
                        return SecurityLevel.OS_SOFTWARE;
                }
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                    && BrowserKeyStore.getDeviceSupportsHardwareKeys()) {
                return SecurityLevel.STRONGBOX;
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                    && keyInfo.isInsideSecureHardware()) {
                return SecurityLevel.TRUSTED_ENVIRONMENT;
            }

            return SecurityLevel.OS_SOFTWARE;
        } catch (InvalidKeySpecException | NoSuchAlgorithmException | NoSuchProviderException e) {
            Log.e(TAG, "Could not get key info.", e);
            return SecurityLevel.UNKNOWN;
        }
    }
}
