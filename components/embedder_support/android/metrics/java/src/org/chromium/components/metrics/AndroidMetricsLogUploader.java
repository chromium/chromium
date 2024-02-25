// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.metrics;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Passes UMA logs from native to a java uploader. */
@JNINamespace("metrics")
public class AndroidMetricsLogUploader {
    private static volatile AndroidMetricsLogConsumer sConsumer;

    /**
     * Configures the consumer of logs data submitted via uploadLog, should be called once during
     * start up.
     *
     * @param uploader The consumer of logs data submitted via uploadLog.
     */
    public static void setConsumer(AndroidMetricsLogConsumer consumer) {
        sConsumer = consumer;
    }

    @CalledByNative
    public static int uploadLog(byte[] data, boolean asyncMetricLoggingFeature) {
        final AndroidMetricsLogConsumer consumer = sConsumer;
        if (asyncMetricLoggingFeature) {
            assert consumer != null : "The consumer for android metrics logging was not set";
            return consumer.log(data);
        } else {
            if (consumer != null) {
                return consumer.log(data);
            }
            // If we end up not having an uploader yet it means metric reporting has been
            // attempted too early so return Http Not Found (404) to indicate the resource does
            // not exist yet.
            return 404;
        }
    }
}
