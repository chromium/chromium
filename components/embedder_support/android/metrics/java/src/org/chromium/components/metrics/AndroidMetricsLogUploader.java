// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.metrics;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.function.Consumer;

/**
 * Passes UMA logs from native to a java uploader.
 */
@JNINamespace("metrics")
public class AndroidMetricsLogUploader {
    private static volatile Consumer<byte[]> sUploader;

    /**
     * Configures the consumer of logs data submitted via uploadLog, should be called once during
     * start up.
     *
     * @param uploader The consumer of logs data submitted via uploadLog.
     */
    public static void setUploader(Consumer<byte[]> uploader) {
        sUploader = uploader;
    }

    // TODO(crbug.com/1264425): return status code of the logging operation.
    @CalledByNative
    public static void uploadLog(byte[] data) {
        final Consumer<byte[]> uploader = sUploader;
        if (uploader != null) {
            uploader.accept(data);
        }
    }
}
