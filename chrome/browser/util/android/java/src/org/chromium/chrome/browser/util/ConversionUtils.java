// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

/**
 * A class containing some utility static methods for common conversions.
 */
public class ConversionUtils {
    public static final int BYTES_PER_KILOBYTE = 1024;
    public static final int BYTES_PER_MEGABYTE = 1024 * 1024;
    public static final int BYTES_PER_GIGABYTE = 1024 * 1024 * 1024;
    public static final int KILOBYTES_PER_GIGABYTE = 1024 * 1024;

    public static long bytesToKilobytes(long bytes) {
        return bytes / BYTES_PER_KILOBYTE;
    }

    public static long bytesToMegabytes(long bytes) {
        return bytes / BYTES_PER_MEGABYTE;
    }

    public static long bytesToGigabytes(long bytes) {
        return bytes / BYTES_PER_GIGABYTE;
    }
}
