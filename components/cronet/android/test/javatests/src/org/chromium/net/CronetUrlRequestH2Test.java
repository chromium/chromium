// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

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

/**
 * Test functionality of CronetUrlRequest under H2 test server
 *
 * <p>Ideally, we should only have a single test class that can parameterize the underlying HTTP
 * servers.
 */
// TODO(crbug.com/344966124): Failing when batched, batch this again.
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "Fallback does not support H2")
@DoNotBatch(reason = "crbug/1459563")
public class CronetUrlRequestH2Test {
    private static final String TAG = CronetUrlRequestH2Test.class.getSimpleName();

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    @Before
    public void setUp() throws Exception {
        // TODO(crbug/1490552): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) -> {
                                CronetTestUtil.setMockCertVerifierForTesting(
                                        builder, QuicTestServer.createMockCertVerifier());
                            });
        }
        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
        mTestRule.getTestFramework().startEngine();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "Platform does not support empty buffers yet.")
    public void testUploadWithEmptyBuffersShouldFailUpload() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                Http2TestServer.getEchoStreamUrl(),
                                callback,
                                callback.getExecutor());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        dataProvider.addRead("".getBytes());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(2);

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Bytes read can't be zero except for last chunk!");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "Platform does not support empty buffers yet.")
    public void testUploadWithEmptyBuffersAsyncShouldFailUpload() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                Http2TestServer.getEchoStreamUrl(),
                                callback,
                                callback.getExecutor());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        dataProvider.addRead("".getBytes());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getNumReadCalls()).isEqualTo(2);

        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception received from UploadDataProvider");
        assertThat(callback.mError)
                .hasCauseThat()
                .hasMessageThat()
                .contains("Bytes read can't be zero except for last chunk!");
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "Platform does not support empty buffers yet.")
    public void testUploadWithEmptyBuffersAtEndShouldSucceed() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                Http2TestServer.getEchoStreamUrl(),
                                callback,
                                callback.getExecutor());

        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        dataProvider.addRead("test".getBytes());
        dataProvider.addRead("".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getUploadedLength()).isEqualTo(8);
        // There are only 2 reads because the last read will never be executed
        // because from the networking stack perspective, we read all the content
        // after executing the second read.
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(2);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("testtest");
    }
}
