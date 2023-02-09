// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.metrics;

/**
 * Used by {@link org.chromium.components.metrics.AndroidMetricsLogUploader} to transport logs to
 * the underlying platform.
 */
public interface AndroidMetricsLogConsumer {
    /**
     * Uploads the log to the underlying platform.
     *
     * @return an HTTP status code to indicate the success.
     */
    int log(byte[] data);
}
