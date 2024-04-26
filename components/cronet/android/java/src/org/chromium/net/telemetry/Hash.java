// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.util.Log;

import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Implements the hash encoding algorithm that is used in some Cronet telemetry fields for shoving
 * string or byte array data into a 64-bit integer.
 *
 * <p>The algorithm is defined as:
 *
 * <ol>
 *   <li>Turn the string into a UTF-8 byte array.
 *   <li>Compute the MD5 hash of the byte array.
 *   <li>Take the first 8 bytes of the hash.
 *   <li>Interpret the bytes as a signed big-endian 64-bit integer.
 * </ol>
 *
 * <p>Exception: the encoding of an empty string is zero.
 */
public final class Hash {
    private static final String TAG = CronetLoggerImpl.class.getSimpleName();

    private static MessageDigest getMd5MessageDigest() {
        try {
            return MessageDigest.getInstance("MD5");
        } catch (NoSuchAlgorithmException e) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Error while instantiating messageDigest", e);
            }
            return null;
        }
    }

    private static final MessageDigest MD5_MESSAGE_DIGEST = getMd5MessageDigest();

    /**
     * Turns a byte array into its hashed representation for use in some Cronet telemetry fields.
     *
     * @return The hash of the byte array, or 0 if the string could not be hashed.
     */
    public static long hash(byte[] bytes) {
        return (MD5_MESSAGE_DIGEST == null || bytes == null || bytes.length == 0)
                ? 0
                : ByteBuffer.wrap(MD5_MESSAGE_DIGEST.digest(bytes)).getLong();
    }

    /**
     * Turns a string into its hashed representation for use in some Cronet telemetry fields.
     *
     * @return The hash of the string, or 0 if the string could not be hashed.
     */
    public static long hash(String string) {
        return (MD5_MESSAGE_DIGEST == null || string == null || string.isEmpty())
                ? 0
                : hash(string.getBytes(UTF_8));
    }
}
