// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

/**
 * VariationsUtils provides utility functions for variations classes.
 */
public class VariationsUtils {
    /**
     * gzip-compresses a byte array of data.
     */
    public static byte[] gzipCompress(byte[] uncompressedData) throws IOException {
        ByteArrayOutputStream byteArrayOutputStream =
                new ByteArrayOutputStream(uncompressedData.length);
        GZIPOutputStream gzipOutputStream = new GZIPOutputStream(byteArrayOutputStream);

        gzipOutputStream.write(uncompressedData);

        gzipOutputStream.close();
        return byteArrayOutputStream.toByteArray();
    }

    /**
     * gzip-uncompresses a byte array of data.
     */
    public static byte[] gzipUncompress(byte[] compressedData) throws IOException {
        ByteArrayInputStream byteInputStream = new ByteArrayInputStream(compressedData);
        ByteArrayOutputStream byteOutputStream = new ByteArrayOutputStream();
        GZIPInputStream gzipInputStream = new GZIPInputStream(byteInputStream);

        int bufferSize = 1024;
        int len;
        byte[] buffer = new byte[bufferSize];
        while ((len = gzipInputStream.read(buffer)) != -1) {
            byteOutputStream.write(buffer, 0, len);
        }
        gzipInputStream.close();
        return byteOutputStream.toByteArray();
    }
}
