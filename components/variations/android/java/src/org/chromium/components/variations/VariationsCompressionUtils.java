// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import com.google.protobuf.CodedInputStream;

import org.chromium.base.Log;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

/** VariationsCompressionUtils provides utility functions for variations classes. */
public class VariationsCompressionUtils {
    private static final String TAG = "VariationsUtils";

    public static final String DELTA_COMPRESSION_HEADER = "x-bm";
    public static final String GZIP_COMPRESSION_HEADER = "gzip";

    /** Exception that is raised if the IM header in the seed request contains invalid data. */
    public static class InvalidImHeaderException extends Exception {
        public InvalidImHeaderException(String msg) {
            super(msg);
        }
    }

    /** Exception this is raised if the delta compression cannot be resolved. */
    public static class DeltaPatchException extends Exception {
        public DeltaPatchException(String msg) {
            super(msg);
        }
    }

    /** Class to save instance manipulations. */
    public static class InstanceManipulations {
        public boolean isGzipCompressed;
        public boolean isDeltaCompressed;

        public InstanceManipulations(boolean gzipCompressed, boolean deltaCompressed) {
            isGzipCompressed = gzipCompressed;
            isDeltaCompressed = deltaCompressed;
        }
    }

    /** Parses the instance manipulations header and returns the result. */
    public static InstanceManipulations getInstanceManipulations(String imHeader)
            throws InvalidImHeaderException {
        List<String> manipulations = new ArrayList<String>();
        if (imHeader.length() > 0) {
            // Split header by comma and remove whitespaces.
            manipulations = Arrays.asList(imHeader.split("\\s*,\\s*"));
        }

        int deltaIm = manipulations.indexOf(DELTA_COMPRESSION_HEADER);
        int gzipIm = manipulations.indexOf(GZIP_COMPRESSION_HEADER);

        boolean isDeltaCompressed = deltaIm >= 0;
        boolean isGzipCompressed = gzipIm >= 0;

        int numCompressions = (isDeltaCompressed ? 1 : 0) + (isGzipCompressed ? 1 : 0);
        if (numCompressions != manipulations.size()) {
            throw new InvalidImHeaderException(
                    "Unrecognized instance manipulations in "
                            + imHeader
                            + "; only x-bm and gzip are supported");
        }

        if (isDeltaCompressed && isGzipCompressed && deltaIm > gzipIm) {
            throw new InvalidImHeaderException(
                    "Unsupported instance manipulations order: expected x-bm,gzip, "
                            + "but received gzip,x-bm");
        }
        return new InstanceManipulations(isGzipCompressed, isDeltaCompressed);
    }

    /** gzip-compresses a byte array of data. */
    public static byte[] gzipCompress(byte[] uncompressedData) throws IOException {
        try (ByteArrayOutputStream byteArrayOutputStream =
                        new ByteArrayOutputStream(uncompressedData.length);
                GZIPOutputStream gzipOutputStream = new GZIPOutputStream(byteArrayOutputStream)) {
            gzipOutputStream.write(uncompressedData);
            gzipOutputStream.close();
            return byteArrayOutputStream.toByteArray();
        }
    }

    /** gzip-uncompresses a byte array of data. */
    public static byte[] gzipUncompress(byte[] compressedData) throws IOException {
        try (ByteArrayInputStream byteInputStream = new ByteArrayInputStream(compressedData);
                ByteArrayOutputStream byteOutputStream = new ByteArrayOutputStream();
                GZIPInputStream gzipInputStream = new GZIPInputStream(byteInputStream)) {
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

    /**  Applies the {@code deltaPatch} to {@code existingSeedData}. */
    public static byte[] applyDeltaPatch(byte[] existingSeedData, byte[] deltaPatch)
            throws DeltaPatchException {
        try (ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            CodedInputStream deltaReader =
                    CodedInputStream.newInstance(ByteBuffer.wrap(deltaPatch));
            while (!deltaReader.isAtEnd()) {
                int value = deltaReader.readUInt32();
                if (value != 0) {
                    // A non-zero value indicates the number of bytes to copy from the patch
                    // stream to the output.
                    byte[] patch = deltaReader.readRawBytes(value);
                    out.write(patch, 0, value);
                } else {
                    // Otherwise, when it's zero, it indicates that it's followed by a pair of
                    // numbers - {@code offset} and {@code length} that specify a range of data
                    // to copy from {@code existingSeedData}.
                    int offset = deltaReader.readUInt32();
                    int length = deltaReader.readUInt32();
                    // addExact raises ArithmeticException if the sum overflows.
                    byte[] copy =
                            Arrays.copyOfRange(
                                    existingSeedData, offset, Math.addExact(offset, length));
                    out.write(copy, 0, length);
                }
            }
            return out.toByteArray();
        } catch (IOException | ArithmeticException | ArrayIndexOutOfBoundsException e) {
            Log.w(TAG, "Delta patch failed.", e);
            throw new DeltaPatchException("Failed to delta patch variations seed");
        }
    }
}
