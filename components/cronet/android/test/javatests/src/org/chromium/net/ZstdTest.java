// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeFalse;

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
import org.chromium.net.CronetTestRule.BoolFlag;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.Flags;
import org.chromium.net.CronetTestRule.IgnoreFor;

import java.util.Arrays;

/** Simple test for zstd support. */
@RunWith(AndroidJUnit4.class)
@DoNotBatch(
        reason =
                "Overriding feature flags via CronetTestRule.Flags can only be done once. Do not"
                        + " batch to have different values per test")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "The fallback implementation doesn't support zstd")
public class ZstdTest {
    private static final String ENABLE_ZSTD_FLAG_NAME = "ChromiumBaseFeature_EnableZstdV2";

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
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
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @SmallTest
    public void testZstdNotAdvertisedByDefault() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).doesNotContain("zstd");
    }

    @Test
    @SmallTest
    @Flags(boolFlags = {@BoolFlag(name = ENABLE_ZSTD_FLAG_NAME, value = true)})
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "This feature flag has not reached platform Cronet yet")
    public void testZstdAdvertisedWhenEnableZstdExperimentEnabled() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).contains("accept-encoding: gzip, deflate, zstd");
    }

    @Test
    @SmallTest
    @Flags(boolFlags = {@BoolFlag(name = ENABLE_ZSTD_FLAG_NAME, value = true)})
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "This feature flag has not reached platform Cronet yet")
    public void testZstdDecodedWhenEnableZstdExperimentEnabled() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getServeSimpleZstdResponse();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        String expectedResponse = "The quick brown fox jumps over the lazy dog\n";
        assertThat(callback.mResponseAsString).isEqualTo(expectedResponse);
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("content-encoding", Arrays.asList("zstd"));
    }

    @Test
    @SmallTest
    public void testZstdNotDecodedByDefault() throws Exception {
        // HttpEngine within Android's 14 emulator image does not know about zstd, so it does not
        // fail decompression. See https://crbug.com/410771958.
        // We could check for that behavior, but there is little value in testing something that
        // cannot be changed. So, skip the test instead.
        assumeFalse(
                Build.VERSION.SDK_INT == Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                        && mTestRule.implementationUnderTest()
                                == CronetImplementation.AOSP_PLATFORM);
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getServeSimpleZstdResponse();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in CronetUrlRequest: net::ERR_CONTENT_DECODING_FAILED");
    }

    private TestUrlRequestCallback startAndWaitForComplete(String url) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        return callback;
    }
}
