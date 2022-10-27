// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ParcelFileDescriptor;

import java.io.File;
import java.nio.ByteBuffer;

/**
 * Provides implementations of {@link UploadDataProvider} for common use cases.
 *
 * @deprecated use {@link org.chromium.net.apihelpers.UploadDataProviders} instead
 */
@Deprecated
public final class UploadDataProviders {
    /**
     * Uploads an entire file.
     *
     * @param file The file to upload
     * @return A new UploadDataProvider for the given file
     */
    public static UploadDataProvider create(final File file) {
        return org.chromium.net.apihelpers.UploadDataProviders.create(file);
    }

    /**
     * Uploads an entire file, closing the descriptor when it is no longer needed.
     *
     * @param fd The file descriptor to upload
     * @return A new UploadDataProvider for the given file descriptor
     * @throws IllegalArgumentException if {@code fd} is not a file.
     */
    public static UploadDataProvider create(final ParcelFileDescriptor fd) {
        return org.chromium.net.apihelpers.UploadDataProviders.create(fd);
    }

    /**
     * Uploads a ByteBuffer, from the current {@code buffer.position()} to {@code buffer.limit()}
     *
     * @param buffer The data to upload
     * @return A new UploadDataProvider for the given buffer
     */
    public static UploadDataProvider create(ByteBuffer buffer) {
        return org.chromium.net.apihelpers.UploadDataProviders.create(buffer);
    }

    /**
     * Uploads {@code length} bytes from {@code data}, starting from {@code offset}
     *
     * @param data Array containing data to upload
     * @param offset Offset within data to start with
     * @param length Number of bytes to upload
     * @return A new UploadDataProvider for the given data
     */
    public static UploadDataProvider create(byte[] data, int offset, int length) {
        return org.chromium.net.apihelpers.UploadDataProviders.create(data, offset, length);
    }

    /**
     * Uploads the contents of {@code data}
     *
     * @param data Array containing data to upload
     * @return A new UploadDataProvider for the given data
     */
    public static UploadDataProvider create(byte[] data) {
        return org.chromium.net.apihelpers.UploadDataProviders.create(data);
    }

    // Prevent instantiation
    private UploadDataProviders() {}
}
