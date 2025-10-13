// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.one_time_tokens.backend.sms;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Tests that dispatcher bridge calls reach the OTP fetcher. */
@RunWith(BaseRobolectricTestRunner.class)
public class AndroidSmsOtpFetchDispatcherBridgeTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AndroidSmsOtpFetchReceiverBridge mReceiverBridgeMock;
    @Mock private AndroidSmsOtpFetcher mSmsOtpFetcherMock;
    @Mock private AndroidSmsOtpFetcherFactory mAndroidSmsOtpFetcherFactoryMock;

    private AndroidSmsOtpFetchDispatcherBridge mDispatcherBridge;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        mDispatcherBridge =
                new AndroidSmsOtpFetchDispatcherBridge(mReceiverBridgeMock, mSmsOtpFetcherMock);
    }

    @Test
    public void testBridgeNotCreatedWhenThereIsNoFetcher() {
        when(mAndroidSmsOtpFetcherFactoryMock.createSmsOtpFetcher()).thenReturn(null);
        AndroidSmsOtpFetcherFactory.setFactoryForTesting(mAndroidSmsOtpFetcherFactoryMock);
        assertNull(AndroidSmsOtpFetchDispatcherBridge.create(mReceiverBridgeMock));
    }

    @Test
    public void testRetrieveSmsOtpCallsReceiverBridgeOnSuccess() {
        final HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Autofill.OneTimeTokens.Backend.Sms.Success", true)
                        .expectAnyRecord("Autofill.OneTimeTokens.Backend.Sms.SuccessLatency")
                        .build();

        mDispatcherBridge.retrieveSmsOtp();
        ArgumentCaptor<Callback<String>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mSmsOtpFetcherMock).retrieveSmsOtp(successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        String kOtpValue = "123456";
        successCallback.getValue().onResult(kOtpValue);
        verify(mReceiverBridgeMock).onOtpValueRetrieved(kOtpValue);

        watcher.assertExpected();
    }

    @Test
    public void testRetrieveSmsOtpCallsReceiverBridgeOnFailure() {
        final HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Autofill.OneTimeTokens.Backend.Sms.Success", false)
                        .expectIntRecord(
                                "Autofill.OneTimeTokens.Backend.Sms.APIError",
                                CommonStatusCodes.TIMEOUT)
                        .expectAnyRecord("Autofill.OneTimeTokens.Backend.Sms.ErrorLatency")
                        .build();

        mDispatcherBridge.retrieveSmsOtp();
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mSmsOtpFetcherMock).retrieveSmsOtp(any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        ApiException receivedException = new ApiException(new Status(CommonStatusCodes.TIMEOUT));
        failureCallback.getValue().onResult(receivedException);
        verify(mReceiverBridgeMock).onOtpValueRetrievalError(receivedException);

        watcher.assertExpected();
    }
}
