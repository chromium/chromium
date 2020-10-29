// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.metrics;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Consumer;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;

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

    @CalledByNative
    public static void uploadLog(byte[] data) throws InvalidProtocolBufferException {
        final Consumer<byte[]> uploader = sUploader;
        if (uploader != null) {
            // Speculative validity checks to see why WebView UMA (and probably other embedders of
            // this component) are missing system_profiles for a small fraction of records.
            // TODO(https://crbug.com/1081925): remove entirely when we figure out the issue.
            if (data.length == 0) {
                throw new RuntimeException("UMA log is completely empty");
            }
            ChromeUserMetricsExtension log = ChromeUserMetricsExtension.parseFrom(data);
            if (!log.hasSystemProfile()) {
                throw new RuntimeException("UMA log is missing a system_profile");
            }
            uploader.accept(data);
        }
    }
}
