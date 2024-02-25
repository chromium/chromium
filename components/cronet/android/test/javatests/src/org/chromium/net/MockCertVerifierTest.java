// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

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

/** Unit tests for {@code MockCertVerifier}. */
@RunWith(AndroidJUnit4.class)
@DoNotBatch(reason = "crbug/1459563")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason = "MockCertVerifier is supported only by the native implementation")
public class MockCertVerifierTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    @Before
    public void setUp() throws Exception {
        // Load library first to create MockCertVerifier.
        System.loadLibrary("cronet_tests");

        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @SmallTest
    public void testRequest_failsWithoutMockVerifierBeforeNougat() {
        assume().that(Build.VERSION.SDK_INT).isLessThan(Build.VERSION_CODES.N);
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError).hasMessageThat().contains("ERR_CERT_AUTHORITY_INVALID");
    }

    @Test
    @SmallTest
    public void testRequest_passesWithMockVerifierBeforeNougat() {
        assume().that(Build.VERSION.SDK_INT).isLessThan(Build.VERSION_CODES.N);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                CronetTestUtil.setMockCertVerifierForTesting(
                                        builder,
                                        MockCertVerifier.createFreeForAllMockCertVerifier()));

        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    @Test
    @SmallTest
    public void testRequest_passesWithoutMockVerifierAfterMarshmallow() {
        assume().that(Build.VERSION.SDK_INT).isGreaterThan(Build.VERSION_CODES.M);
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    private TestUrlRequestCallback startAndWaitForComplete(String url) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .startEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        return callback;
    }
}
