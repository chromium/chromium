// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.junit.Assert;

import java.nio.ByteBuffer;

/** Utility functions for tests using ByteBuffers. */
public final class ByteBufferTestUtils {
    /**
     * Verify a ByteBuffer is equal to a byte array.
     * @param expected bytes expected in ByteBuffer, stored as a byte array.
     * @param actual ByteBuffer found to be compared to the byte array.
     */
    public static void verifyByteBuffer(byte[] expected, ByteBuffer actual) {
        Assert.assertEquals(expected.length, actual.limit());
        for (int i = 0; i < actual.limit(); i++) {
            Assert.assertEquals(expected[i], actual.get());
        }
    }

    /**
     * Verifies two ByteBuffers are equal.
     *
     * @param expected ByteBuffer to compare to.
     * @param actual ByteBuffer acquired by test code.
     */
    public static void verifyByteBuffer(ByteBuffer expected, ByteBuffer actual) {
        expected.rewind();
        actual.rewind();
        Assert.assertTrue(expected.equals(actual));
    }
}
