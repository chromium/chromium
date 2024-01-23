// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.test.ServerCertificate;

/** Test functionality of CronetUrlRequest when SSL is enabled. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
public class CronetUrlRequestHTTPSTest {
    private static final String TAG = CronetUrlRequestHTTPSTest.class.getSimpleName();

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    @Before
    public void setUp() {
        NativeTestServer.startNativeTestServerWithHTTPS(
                mTestRule.getTestFramework().getContext(), ServerCertificate.CERT_EXPIRED);
    }

    @After
    public void tearDown() {
        NativeTestServer.shutdownNativeTestServer();
    }

    /**
     * Tests that an SSL cert error with upload will be reported via {@link
     * UrlRequest.Callback#onFailed}.
     */
    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "crbug.com/1495320: Refactor error checking")
    public void testSSLCertificateError() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getFileURL("/"), callback, callback.getExecutor());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(callback.getResponseInfo()).isNull();
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in CronetUrlRequest: net::ERR_CERT_DATE_INVALID");
        mTestRule.assertCronetInternalErrorCode((NetworkException) callback.mError, -201);
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
    }
}
