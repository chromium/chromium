// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.security.keystore.KeyProperties;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.Signature;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.ECPoint;

@RunWith(BaseRobolectricTestRunner.class)
public class BrowserBoundKeyTest {

    @Test
    public void testReturnsIdentifier() {
        byte[] identifier = new byte[] {0x10, 0x11, 0x12, 0x13};
        BrowserBoundKey browserBoundKey = new BrowserBoundKey(identifier, /* keyPair= */ null);

        assertArrayEquals(new byte[] {0x10, 0x11, 0x12, 0x13}, browserBoundKey.getIdentifier());
    }

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
        BrowserBoundKey browserBoundKey =
                new BrowserBoundKey(/* identifier= */ new byte[0], keyPair);
        byte[] clientData = {0x01, 0x02, 0x03, 0x04};

        byte[] actualSignature = browserBoundKey.sign(clientData);

        Signature signature = Signature.getInstance("SHA256withECDSA");
        signature.initVerify(keyPair.getPublic());
        signature.update(clientData);
        assertTrue(signature.verify(actualSignature));
    }

    /** The CBOR byte denoting a bytestring with uint8_t length, then followed by the bytestring. */
    private static final byte CBOR_BYTESTRING_UINT8 = 0b010_11000;

    /** Test that coordinates of 32 bytes with and without leading zeros are encoded. */
    @Test
    public void testEncodeCoseKeyWith256BitEcPublicKey() {
        // BigInteger serializes this to a 33 byte array. The first byte will be 0 followed by the
        // the same bytes listed here.
        final byte[] coordinate_with_leading_one =
                new byte[] {
                    -128, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
                };
        // BigInteger serializes this to 32 bytes.
        final byte[] coordinate_with_leading_zero =
                new byte[] {
                    127, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
                    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64
                };
        ECPublicKey publicKey =
                mockEcPublicKey(
                        new BigInteger(/* signum= */ 1, coordinate_with_leading_one),
                        new BigInteger(/* signum= */ 1, coordinate_with_leading_zero));
        BrowserBoundKey bbk =
                new BrowserBoundKey(
                        /* identifier= */ new byte[0], new KeyPair(publicKey, /* private= */ null));

        byte[] encodedKey = bbk.getPublicKeyAsCoseKey();

        assertNotNull(encodedKey);
        assertEncodedCoseKey(encodedKey, coordinate_with_leading_one, coordinate_with_leading_zero);
    }

    /**
     * Test that a coordinate that serializes to less than 32 bytes encodes as 32 bytes.
     *
     * <p>BigInteger.toByteArray() will encode fewer than 32 bytes when the integer does not require
     * 32 bytes to be encoded; however, the COSE key format needs 32 bytes with leading zeros.
     */
    @Test
    public void testEncodeCoseKeyAddsLeadingZerosForSmallCoordinates() {
        final byte[] small_coordinate =
                new byte[] {
                    -128, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                    25, 26, 27, 28, 29, 30, 31, 32
                };
        final byte[] small_coordinate_encoded =
                new byte[] {
                    0, 0, 0, 0, -128, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
                };
        ECPublicKey publicKey =
                mockEcPublicKey(
                        new BigInteger(/* signum= */ 1, small_coordinate),
                        new BigInteger(/* signum= */ 1, small_coordinate));
        BrowserBoundKey bbk =
                new BrowserBoundKey(
                        /* identifier= */ new byte[0], new KeyPair(publicKey, /* private= */ null));

        byte[] encodedKey = bbk.getPublicKeyAsCoseKey();

        assertNotNull(encodedKey);
        assertEncodedCoseKey(encodedKey, small_coordinate_encoded, small_coordinate_encoded);
    }

    /** Test that a null is returned for a unexpected negative coordinates. */
    @Test
    public void testEncodeCoseKeyReturnsNullOnNegativeCoordinate() {
        BigInteger negative_coordinate = BigInteger.valueOf(-1);
        ECPublicKey publicKey = mockEcPublicKey(negative_coordinate, negative_coordinate);
        BrowserBoundKey bbk =
                new BrowserBoundKey(
                        /* identifier= */ new byte[0], new KeyPair(publicKey, /* private= */ null));

        byte[] encodedKey = bbk.getPublicKeyAsCoseKey();

        assertNull(encodedKey);
    }

    /** Test that a null is returned for an unexpected coordinate size. */
    @Test
    public void testEncodeCoseKeyReturnsNullOnUnexpectedCoordinateSize() {
        final byte[] large_coordinate_bytes = new byte[257];
        large_coordinate_bytes[0] = 1;
        final BigInteger large_coordinate = new BigInteger(/* signum= */ 1, large_coordinate_bytes);
        ECPublicKey publicKey = mockEcPublicKey(large_coordinate, large_coordinate);
        BrowserBoundKey bbk =
                new BrowserBoundKey(
                        /* identifier= */ new byte[0], new KeyPair(publicKey, /* private= */ null));

        byte[] encodedKey = bbk.getPublicKeyAsCoseKey();

        assertNull(encodedKey);
    }

    /** Create a mock ECPublicKey with the given bytes for the curve coordinates. */
    ECPublicKey mockEcPublicKey(BigInteger x, BigInteger y) {
        ECPublicKey publicKey = mock(ECPublicKey.class);
        when(publicKey.getW()).thenReturn(new ECPoint(x, y));
        return publicKey;
    }

    /**
     * Asserts an encoded COSE key with curve P-256 restricted to ES256.
     *
     * @param expectedX The encoded 32 byte x-coordinate.
     * @param expectedY The encoded 32 byte y-coordinate.
     */
    private static void assertEncodedCoseKey(byte[] actualKey, byte[] expectedX, byte[] expectedY) {
        ByteBuffer actualKeyBuffer = ByteBuffer.wrap(actualKey);
        readAndAssertEqual(actualKeyBuffer, COSE_KEY_ES256_PREFIX);
        readAndAssertEqual(actualKeyBuffer, COSE_KEY_X_COORDINATE_PREFIX);
        readAndAssertEqual(actualKeyBuffer, expectedX);
        readAndAssertEqual(actualKeyBuffer, COSE_KEY_Y_COORDINATE_PREFIX);
        readAndAssertEqual(actualKeyBuffer, expectedY);
        assertEquals(0, actualKeyBuffer.remaining());
    }

    private static final byte CBOR_MAP_5 = (byte) 0b101_00101;
    private static final byte CBOR_1 = 0b000_00001;
    private static final byte CBOR_2 = 0b000_00010;
    private static final byte CBOR_3 = 0b000_00011;
    private static final byte CBOR_NEGATIVE_1 = 0b001_00000;
    private static final byte CBOR_NEGATIVE_2 = 0b001_00001;
    private static final byte CBOR_NEGATIVE_3 = 0b001_00010;
    private static final byte CBOR_NEGATIVE_7 = 0b001_00110;

    private static final byte[] COSE_KEY_ES256_PREFIX =
            new byte[] {
                CBOR_MAP_5, // The COSE key is a map.
                CBOR_1, // The key type is
                CBOR_2, // EC2.
                CBOR_3, // Restricted to algorithm
                CBOR_NEGATIVE_7, // ES256.
                CBOR_NEGATIVE_1, // The curve type is
                CBOR_1, // P-256
            };
    private static final byte[] COSE_KEY_X_COORDINATE_PREFIX =
            new byte[] {
                CBOR_NEGATIVE_2, // The X-coordinate label.
                CBOR_BYTESTRING_UINT8, // A byte string of
                32, // size 32 bytes.
            };
    private static final byte[] COSE_KEY_Y_COORDINATE_PREFIX =
            new byte[] {
                CBOR_NEGATIVE_3, // The Y-coordinate label.
                CBOR_BYTESTRING_UINT8, // A byte string of
                32, // size 32 bytes.
            };

    /** Reads the expected length from the buffer and asserts the arrays are equal. */
    private static void readAndAssertEqual(ByteBuffer actualBuffer, byte[] expected) {
        assertTrue(actualBuffer.remaining() >= expected.length);
        byte[] actual = new byte[expected.length];
        actualBuffer.get(actual);
        assertArrayEquals(expected, actual);
    }
}
