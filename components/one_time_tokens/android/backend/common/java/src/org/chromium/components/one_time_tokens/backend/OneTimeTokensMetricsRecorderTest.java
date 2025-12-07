// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.one_time_tokens.backend;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Unit tests for {@link OneTimeTokensMetricsRecorder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OneTimeTokensMetricsRecorderTest {
    private static final String HISTOGRAM_BASE = "TestHistogram";
    private OneTimeTokensMetricsRecorder mMetricsRecorder;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        mMetricsRecorder = new OneTimeTokensMetricsRecorder(HISTOGRAM_BASE);
    }

    @Test
    public void testRecordMetrics_success() {
        final HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.Success", true)
                        .expectAnyRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.SuccessLatency")
                        .build();

        mMetricsRecorder.recordMetrics(null);
        ShadowLooper.runUiThreadTasks();
        watcher.assertExpected();
    }

    @Test
    public void testRecordMetrics_oneTimeTokensBackendException() {
        final HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.Success", false)
                        .expectAnyRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.ErrorLatency")
                        .expectIntRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.ErrorCode",
                                OneTimeTokensBackendErrorCode.GMSCORE_VERSION_NOT_SUPPORTED)
                        .build();

        mMetricsRecorder.recordMetrics(
                new OneTimeTokensBackendException(
                        "test error", OneTimeTokensBackendErrorCode.GMSCORE_VERSION_NOT_SUPPORTED));
        ShadowLooper.runUiThreadTasks();
        watcher.assertExpected();
    }

    @Test
    public void testRecordMetrics_apiException() {
        final HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.Success", false)
                        .expectAnyRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.ErrorLatency")
                        .expectIntRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.APIError",
                                CommonStatusCodes.TIMEOUT)
                        .build();

        mMetricsRecorder.recordMetrics(new ApiException(new Status(CommonStatusCodes.TIMEOUT)));
        ShadowLooper.runUiThreadTasks();
        watcher.assertExpected();
    }

    @Test
    public void testRecordMetrics_genericException() {
        final HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.Success", false)
                        .expectAnyRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.ErrorLatency")
                        .expectIntRecord(
                                "Autofill.OneTimeTokens.Backend.TestHistogram.APIError",
                                CommonStatusCodes.ERROR)
                        .build();

        mMetricsRecorder.recordMetrics(new Exception("test"));
        ShadowLooper.runUiThreadTasks();
        watcher.assertExpected();
    }
}
