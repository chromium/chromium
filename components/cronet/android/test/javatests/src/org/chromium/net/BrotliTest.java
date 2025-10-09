// Copyright 2017 The Chromium Authors
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
import org.chromium.net.CronetTestRule.BoolFlag;
import org.chromium.net.CronetTestRule.Flags;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.test.ServerCertificate;

import java.util.Arrays;

/** Simple test for Brotli support. */
@RunWith(AndroidJUnit4.class)
@RequiresMinApi(5) // Brotli support added in API version 5: crrev.com/465216
@DoNotBatch(
        reason =
                "Overriding feature flags via CronetTestRule.Flags can only be done once. Do not"
                        + " batch to have different values per test")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "The fallback implementation doesn't support Brotli")
public class BrotliTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private CronetEngine mCronetEngine;
    private NativeTestServer mNativeTestServer;

    @Before
    public void setUp() throws Exception {
        mNativeTestServer =
                NativeTestServer.createNativeTestServerWithHTTPS(
                        mTestRule.getTestFramework().getContext(), ServerCertificate.CERT_OK);
        mNativeTestServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mNativeTestServer.close();
    }

    @Test
    @SmallTest
    public void testBrotliAdvertised() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableBrotli(true);
                        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = mNativeTestServer.getEchoAllHeadersURL();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).contains("Accept-Encoding: gzip, deflate, br");
    }

    @Test
    @SmallTest
    @Flags(
            boolFlags = {
                @BoolFlag(
                        name = CronetUrlRequestContext.ALWAYS_ENABLE_BROTLI_FLAG_NAME,
                        value = true)
            })
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "This feature flag has not reached platform Cronet yet")
    public void testBrotliAdvertisedWhenAlwaysEnableBrotliExperimentEnabled_default()
            throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = mNativeTestServer.getEchoAllHeadersURL();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).contains("Accept-Encoding: gzip, deflate, br");
    }

    @Test
    @SmallTest
    @Flags(
            boolFlags = {
                @BoolFlag(
                        name = CronetUrlRequestContext.ALWAYS_ENABLE_BROTLI_FLAG_NAME,
                        value = true)
            })
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "This feature flag has not reached platform Cronet yet")
    public void testBrotliAdvertisedWhenAlwaysEnableBrotliExperimentEnabled_explicitlyDisabled()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableBrotli(false);
                        });
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = mNativeTestServer.getEchoAllHeadersURL();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).contains("Accept-Encoding: gzip, deflate, br");
    }

    @Test
    @SmallTest
    public void testBrotliNotAdvertised() throws Exception {
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = mNativeTestServer.getEchoAllHeadersURL();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).doesNotContain("br");
    }

    @Test
    @SmallTest
    public void testBrotliDecoded() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableBrotli(true);
                        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = mNativeTestServer.getUseEncodingURL("brotli");
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        String expectedResponse = "The quick brown fox jumps over the lazy dog";
        assertThat(callback.mResponseAsString).isEqualTo(expectedResponse);
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("content-encoding", Arrays.asList("br"));
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
