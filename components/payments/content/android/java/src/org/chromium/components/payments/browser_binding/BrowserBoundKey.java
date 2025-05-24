// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.Signature;
import java.security.SignatureException;
import java.security.interfaces.ECPublicKey;
import java.util.Arrays;

/**
 * A browser bound key pair for a matching passkey.
 *
 * <p>In SecurePaymentConfirmation get assertion requests, a browser bound key can be used to
 * provide an additional signature over the client data.
 */
@NullMarked
public class BrowserBoundKey {

    /** The logging tag for this class. */
    private static final String TAG = "SpcBbKey";

    /** The signature algorithm to use for signing with the browser bound key. */
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
     * Creates a browser bound key given the identifier and keyPair.
     *
     * @param identifier The identifier of the browser bound key.
     * @param keyPair The KeyPair of this browser bound key obtained from a KeyStore.
     */
    BrowserBoundKey(byte[] identifier, KeyPair keyPair) {
        mIdentifier = identifier;
        mKeyPair = keyPair;
    }

    @CalledByNative
    public byte @Nullable [] sign(byte[] clientData) {
        Signature signature;
        try {
            signature = Signature.getInstance(SHA256_WITH_ECDSA);
        } catch (NoSuchAlgorithmException e) {
            // TODO(crbug.com/377278827): Eventually we will want to let native code know that we
            // can't do our job.
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

    /**
     * Returns the public key encoded as as COSE_Key including its algorithm type and parameters.
     *
     * <p>Only ES256 with curve P-256 is implemented, and the key must be 256 bits.
     *
     * <p>See credentialPublicKey in
     * https://www.w3.org/TR/webauthn-2/#sctn-attested-credential-data.
     */
    @CalledByNative
    @JniType("std::vector<uint8_t>")
    public byte @Nullable [] getPublicKeyAsCoseKey() {
        if (!(mKeyPair.getPublic() instanceof ECPublicKey)) {
            return null;
        }
        return encodeCoseKeyWithEs256SignatureAlgorithm((ECPublicKey) mKeyPair.getPublic());
    }

    /** Returns the identifier of this browser bound key. */
    @CalledByNative
    @JniType("std::vector<uint8_t>")
    public byte[] getIdentifier() {
        return mIdentifier;
    }

    KeyPair getKeyPairForTesting() {
        return mKeyPair;
    }

    private static class UnsupportedCborEncodingException extends Exception {
        private UnsupportedCborEncodingException(String message) {
            super(message);
        }
    }

    /**
     * Encodes the given public key as a COSE key with ES256 as the signature algorithm.
     *
     * <p>See credentialPublicKey, https://www.w3.org/TR/webauthn-2/#credentialpublickey.
     *
     * @param ecPublicKey An elliptic curve public key. The bit size must be 256.
     */
    private static byte @Nullable [] encodeCoseKeyWithEs256SignatureAlgorithm(
            ECPublicKey ecPublicKey) {
        try {
            final int coseKeySizeForEs256 = 77;
            ByteBuffer coseKeyBuffer = ByteBuffer.allocate(coseKeySizeForEs256);
            // Begin a map of 5 pairs: For an ES256 key we need the key type (kty), algorithm (alg),
            // curve type (crv), X-coordinate (x), and Y-coordinate (y). As in the WebAuthn spec
            // other (optional) labels are not included.
            putCborMajorTypeWithValue(coseKeyBuffer, CBOR_MAJOR_TYPE_MAP, 5); // Map with 5 pairs.

            putCborInteger(coseKeyBuffer, 1); // Key type (kty)
            putCborInteger(coseKeyBuffer, 2); // Elliptic curve key. See
            // https://www.iana.org/assignments/cose/cose.xhtml#key-type

            putCborInteger(coseKeyBuffer, 3); // Key restricted to algorithm (alg)
            putCborInteger(coseKeyBuffer, COSE_ALGORITHM_ES256);

            // The remaining pairs are key type parameters. See
            // https://www.iana.org/assignments/cose/cose.xhtml#key-type-parameters

            putCborInteger(coseKeyBuffer, -1); // Key type parameter: Curve Type (crv)
            putCborInteger(coseKeyBuffer, 1); // P-256. See
            // https://www.iana.org/assignments/cose/cose.xhtml#elliptic-curves

            putCborInteger(coseKeyBuffer, -2); // Key type parameter: X-coordinate (x)
            putCoordinateAsCborByteString(coseKeyBuffer, ecPublicKey.getW().getAffineX());

            putCborInteger(coseKeyBuffer, -3); // Key type parameter: Y-coordinate (y)
            putCoordinateAsCborByteString(coseKeyBuffer, ecPublicKey.getW().getAffineY());

            // The buffer must be filled; otherwise, there is a problem with the encoding.
            assert coseKeyBuffer.position() == coseKeySizeForEs256;
            return coseKeyBuffer.array();
        } catch (IndexOutOfBoundsException | UnsupportedCborEncodingException e) {
            Log.e(TAG, "The browser bound public key could not be encoded.", e);
            return null;
        }
    }

    // The CBOR major types are encoded as integers in the top bits (bits 5-7). The lower bits (bits
    // 0-4) are intentionally 0 here and will be filled by the value. See CBOR Major Types,
    // https://datatracker.ietf.org/doc/html/rfc7049#section-2.1.

    /** CBOR major type 0, a positive integer. */
    private static final byte CBOR_MAJOR_TYPE_POSITIVE_INTEGER = (byte) 0b000_00000;

    /** CBOR major type 1, a negative integer. */
    private static final byte CBOR_MAJOR_TYPE_NEGATIVE_INTEGER = (byte) 0b001_00000;

    /** CBOR major type 2, a byte string. */
    private static final byte CBOR_MAJOR_TYPE_BYTE_STRING = (byte) 0b010_00000;

    /** CBOR major type 5, a map of pairs. */
    private static final byte CBOR_MAJOR_TYPE_MAP = (byte) 0b101_00000;

    /** Writes a CBOR integer value in the range [-256, 255]. */
    private static void putCborInteger(ByteBuffer out, int value)
            throws UnsupportedCborEncodingException {
        if (value >= 0) {
            putCborMajorTypeWithValue(out, CBOR_MAJOR_TYPE_POSITIVE_INTEGER, value);
        } else {
            putCborMajorTypeWithValue(out, CBOR_MAJOR_TYPE_NEGATIVE_INTEGER, -(value + 1));
        }
    }

    /**
     * Serializes a 256-bit coordinate to a CBOR byte string in big endian format.
     *
     * @param coordinate The coordinate of a 256-bit elliptic curve key. Must not be negative.
     */
    private static void putCoordinateAsCborByteString(ByteBuffer out, BigInteger coordinate)
            throws UnsupportedCborEncodingException {
        if (coordinate.signum() == -1) {
            throw new UnsupportedCborEncodingException("The coordinate must non-negative.");
        }
        if (coordinate.bitLength() > 256) {
            throw new UnsupportedCborEncodingException("The coordinate must be 256-bits.");
        }
        byte[] twosComplementBytes = coordinate.toByteArray();
        byte[] unsignedBytes;
        // The twosComplementBytes representation might contain an initial extra zero (0) byte, this
        // byte needs to be ignored when the leading zero would make the byte array larger than 32
        // bytes.
        if (twosComplementBytes.length == 33) {
            unsignedBytes = Arrays.copyOfRange(twosComplementBytes, /* from= */ 1, /* to= */ 33);
        } else {
            unsignedBytes = new byte[32];
            int start = 32 - twosComplementBytes.length;
            // The upper bytes stay initialized to 0, fill the remaining bytes with the
            // twosComplementBytes.
            System.arraycopy(
                    /* src= */ twosComplementBytes,
                    /* srcPos= */ 0,
                    /* dest= */ unsignedBytes,
                    /* destPos= */ start,
                    /* length= */ twosComplementBytes.length);
        }
        putCborMajorTypeWithValue(out, CBOR_MAJOR_TYPE_BYTE_STRING, unsignedBytes.length);
        out.put(unsignedBytes);
    }

    /**
     * Writes small CBOR values with the given major type.
     *
     * @param value The value to write. Must be less than 256.
     */
    private static void putCborMajorTypeWithValue(ByteBuffer out, byte majorType, int value)
            throws UnsupportedCborEncodingException {
        // See CBOR Major Types: https://datatracker.ietf.org/doc/html/rfc7049#section-2.1.
        assert value >= 0;
        if (value <= 23) {
            out.put((byte) (majorType | value));
        } else if (value <= 255) {
            out.put((byte) (majorType | 24)); // A uint8_t follows (1 byte)
            out.put((byte) value);
        } else {
            throw new UnsupportedCborEncodingException(
                    "Writing values larger than 255 is unimplemented.");
        }
    }
}
