// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.one_time_tokens.backend.sms;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.one_time_tokens.backend.OneTimeTokensMetricsRecorder;

/**
 * Java-counterpart of the native AndroidSmsOtpFetchDispatcherBridge. It's part of the OTP value
 * fetching backend that forwards retrieval requests to a downstream implementation.
 */
@NullMarked
class AndroidSmsOtpFetchDispatcherBridge {
    private static final String HISTOGRAM_SUFFIX = "Sms";
    private final AndroidSmsOtpFetchReceiverBridge mReceiverBridge;
    private final AndroidSmsOtpFetcher mOtpFetcher;

    AndroidSmsOtpFetchDispatcherBridge(
            AndroidSmsOtpFetchReceiverBridge receiverBridge, AndroidSmsOtpFetcher otpFetcher) {
        mReceiverBridge = receiverBridge;
        mOtpFetcher = otpFetcher;
    }

    @CalledByNative
    static @Nullable AndroidSmsOtpFetchDispatcherBridge create(
            AndroidSmsOtpFetchReceiverBridge receiverBridge) {
        AndroidSmsOtpFetcher otpFetcher =
                AndroidSmsOtpFetcherFactory.getInstance().createSmsOtpFetcher();
        if (otpFetcher == null) {
            return null;
        }
        return new AndroidSmsOtpFetchDispatcherBridge(receiverBridge, otpFetcher);
    }

    @CalledByNative
    void retrieveSmsOtp() {
        // Create a new metrics recorder for this retrieval attempt. The recorder
        // will be used to log the outcome and latency of the operation.
        final OneTimeTokensMetricsRecorder metricsRecorder =
                new OneTimeTokensMetricsRecorder(HISTOGRAM_SUFFIX);
        mOtpFetcher.retrieveSmsOtp(
                otpValue -> {
                    metricsRecorder.recordMetrics(null);
                    mReceiverBridge.onOtpValueRetrieved(otpValue);
                },
                exception -> {
                    metricsRecorder.recordMetrics(exception);
                    mReceiverBridge.onOtpValueRetrievalError(exception);
                });
    }
}
