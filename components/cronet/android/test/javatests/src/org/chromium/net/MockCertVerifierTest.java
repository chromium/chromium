// Copyright 2015 The Chromium Authors
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
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.impl.CronetLibraryLoader;
import org.chromium.net.test.Protocol;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.Type;

/** Unit tests for {@code MockCertVerifier}. */
@RunWith(AndroidJUnit4.class)
@DoNotBatch(reason = "crbug/1459563")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason = "MockCertVerifier is supported only by the native implementation")
public class MockCertVerifierTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private NativeTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        // Load library first to create MockCertVerifier.
        CronetLibraryLoader.switchToTestLibrary();
        CronetLibraryLoader.loadLibrary();
        mTestServer =
                new NativeTestServer(
                        mTestRule.getTestFramework().getContext(), Type.HTTPS, Protocol.HTTP2);
        mTestServer.setSSLConfig(ServerCertificate.CERT_OK);
        mTestServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.close();
    }

    @Test
    @SmallTest
    public void testRequest_passesWithMockVerifier() {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                CronetTestUtil.setMockCertVerifierForTesting(
                                        builder,
                                        MockCertVerifier.createFreeForAllMockCertVerifier()));

        String url = mTestServer.getEchoAllHeadersURL();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    @Test
    @SmallTest
    public void testRequest_passesWithoutMockVerifier() {
        String url = mTestServer.getEchoAllHeadersURL();
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
