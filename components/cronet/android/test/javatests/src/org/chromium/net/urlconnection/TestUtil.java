// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;

/** Helper functions and fields used in Cronet's HttpURLConnection tests. */
public class TestUtil {
    static final String UPLOAD_DATA_STRING = "Nifty upload data!";
    static final byte[] UPLOAD_DATA = UPLOAD_DATA_STRING.getBytes();
    static final int REPEAT_COUNT = 100000;

    /** Helper method to extract response body as a string for testing. */
    static String getResponseAsString(HttpURLConnection connection) throws Exception {
        InputStream in = connection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        int b;
        while ((b = in.read()) != -1) {
            out.write(b);
        }
        return out.toString();
    }

    /**
     * Produces a byte array that contains {@code REPEAT_COUNT} of
     * {@code UPLOAD_DATA_STRING}.
     */
    static byte[] getLargeData() {
        byte[] largeData = new byte[REPEAT_COUNT * UPLOAD_DATA.length];
        for (int i = 0; i < REPEAT_COUNT; i++) {
            System.arraycopy(UPLOAD_DATA, 0, largeData, i * UPLOAD_DATA.length, UPLOAD_DATA.length);
        }
        return largeData;
    }

    /**
     * Helper function to check whether {@code data} is a concatenation of
     * {@code REPEAT_COUNT} {@code UPLOAD_DATA_STRING} strings.
     */
    static void checkLargeData(String data) {
        for (int i = 0; i < REPEAT_COUNT; i++) {
            assertThat(
                            data.substring(
                                    UPLOAD_DATA_STRING.length() * i,
                                    UPLOAD_DATA_STRING.length() * (i + 1)))
                    .isEqualTo(UPLOAD_DATA_STRING);
        }
    }
}
