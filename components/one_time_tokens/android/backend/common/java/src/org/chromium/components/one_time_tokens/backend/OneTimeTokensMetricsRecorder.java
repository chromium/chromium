// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.one_time_tokens.backend;

import android.os.SystemClock;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Records metrics for an asynchronous one-time token job. The job is expected to have started when
 * the {@link OneTimeTokensMetricsRecorder} instance is created. Latency is reported in {@link
 * #recordMetrics(Exception) recordMetrics} under that assumption.
 */
@NullMarked
public class OneTimeTokensMetricsRecorder {
    private static final String ONE_TIME_TOKENS_HISTOGRAM_BASE = "Autofill.OneTimeTokens.Backend";
    private static final String SUCCESS_METRIC_NAME = "Success";
    private static final String SUCCESS_LATENCY_METRIC_NAME = "SuccessLatency";
    private static final String ERROR_LATENCY_METRIC_NAME = "ErrorLatency";
    private static final String ERROR_CODE_METRIC_NAME = "ErrorCode";
    private static final String API_ERROR_METRIC_NAME = "APIError";

    private final String mHistogramBase;
    private final long mStartTimeMs;

    public OneTimeTokensMetricsRecorder(String histogramBase) {
        mHistogramBase = histogramBase;
        mStartTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * Records the metrics depending on {@link Exception} provided. Success metric is always
     * reported. Latency is reported separately for successful and failed operations. Error codes
     * are reported for failed operations only.
     *
     * @param exception {@link Exception} instance corresponding to the occurred error
     */
    public void recordMetrics(@Nullable Exception exception) {
        RecordHistogram.recordBooleanHistogram(
                getHistogramName(SUCCESS_METRIC_NAME), exception == null);
        long durationMs = SystemClock.elapsedRealtime() - mStartTimeMs;
        RecordHistogram.recordTimesHistogram(
                getHistogramName(
                        exception == null
                                ? SUCCESS_LATENCY_METRIC_NAME
                                : ERROR_LATENCY_METRIC_NAME),
                durationMs);
        if (exception != null) {
            reportErrorMetrics(exception);
        }
    }

    private String getHistogramName(String metric) {
        return String.join(".", ONE_TIME_TOKENS_HISTOGRAM_BASE, mHistogramBase, metric);
    }

    private void reportErrorMetrics(Exception exception) {
        if (exception instanceof OneTimeTokensBackendException backendException) {
            RecordHistogram.recordEnumeratedHistogram(
                    getHistogramName(ERROR_CODE_METRIC_NAME),
                    backendException.getErrorCode(),
                    OneTimeTokensBackendErrorCode.MAX_VALUE);
            return;
        }

        int errorCode = CommonStatusCodes.ERROR;
        if (exception instanceof ApiException apiException) {
            errorCode = apiException.getStatusCode();
        }
        RecordHistogram.recordSparseHistogram(getHistogramName(API_ERROR_METRIC_NAME), errorCode);
    }
}
