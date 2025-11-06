// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

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

import org.chromium.net.CronetTestFramework;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.NativeTestServer;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UrlRequest;

/** Test e2e behaviours of UserAgent. */
@RunWith(AndroidJUnit4.class)
public class UserAgentIntegrationTest {

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();
    public CronetTestFramework mCronetTestFramework;
    private NativeTestServer mNativeTestServer;

    @Before
    public void setUp() throws Exception {
        mCronetTestFramework = mTestRule.getTestFramework();
        mNativeTestServer =
                NativeTestServer.createNativeTestServer(mCronetTestFramework.getContext());
        mNativeTestServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mNativeTestServer.close();
    }

    @Test
    @SmallTest
    public void testUserAgent_setUserAgent() throws Exception {
        mCronetTestFramework.startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        String headerName = "User-Agent";
        String UA_TEST_STRING = "I'm a teapot";

        var cronetBuilder =
                new NativeCronetProvider(mCronetTestFramework.getContext()).createBuilder();
        cronetBuilder.setUserAgent(UA_TEST_STRING);
        var cronetEngine = cronetBuilder.build();

        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getEchoHeaderURL(headerName),
                        callback,
                        callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(UA_TEST_STRING);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Statically linked specific behaviour")
    public void testUserAgent_NativeCronetSetUserAgentToNullRevertsToDefault() throws Exception {
        mCronetTestFramework.startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        String headerName = "User-Agent";
        String UA_TEST_STRING = "I'm a teapot";

        var cronetBuilder =
                new NativeCronetProvider(mCronetTestFramework.getContext()).createBuilder();
        cronetBuilder.setUserAgent(UA_TEST_STRING);
        cronetBuilder.setUserAgent(null);
        var cronetEngine = cronetBuilder.build();

        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getEchoHeaderURL(headerName),
                        callback,
                        callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo(
                        UserAgentTestUtil.getUserAgentWithPackageName(
                                CronetImplementation.STATICALLY_LINKED));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {
                CronetImplementation.FALLBACK,
                CronetImplementation.STATICALLY_LINKED
            },
            reason = "HttpEngine specific behaviour")
    @RequiresMinAndroidApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testUserAgent_HttpEngineSetUserAgentToNullRevertsToDefault() throws Exception {
        mCronetTestFramework.startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        String headerName = "User-Agent";
        String UA_TEST_STRING = "I'm a teapot";

        var cronetBuilder =
                new HttpEngineNativeProvider(mCronetTestFramework.getContext()).createBuilder();
        cronetBuilder.setUserAgent(UA_TEST_STRING);
        cronetBuilder.setUserAgent(null);
        var cronetEngine = cronetBuilder.build();

        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getEchoHeaderURL(headerName),
                        callback,
                        callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo(
                        UserAgentTestUtil.getUserAgentWithAndroidHttpClient(
                                CronetImplementation.AOSP_PLATFORM));
    }

    @Test
    @SmallTest
    public void testDefaultUserAgent_noLegacyOptIn() throws Exception {
        mCronetTestFramework.startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "User-Agent";
        UrlRequest.Builder builder =
                mCronetTestFramework
                        .getEngine()
                        .newUrlRequestBuilder(
                                mNativeTestServer.getEchoHeaderURL(headerName),
                                callback,
                                callback.getExecutor());
        builder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo(
                        UserAgentTestUtil.getDefaultUserAgent(mTestRule.implementationUnderTest()));
    }

    @Test
    @SmallTest
    public void testDefaultUserAgent_legacyOptIn() throws Exception {
        mCronetTestFramework.interceptContext(
                UserAgentTestUtil.getContextInterceptorWithLegacyUserAgent(true));
        mCronetTestFramework.startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "User-Agent";
        UrlRequest.Builder builder =
                mCronetTestFramework
                        .getEngine()
                        .newUrlRequestBuilder(
                                mNativeTestServer.getEchoHeaderURL(headerName),
                                callback,
                                callback.getExecutor());
        builder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo(
                        UserAgentTestUtil.getUserAgentWithPackageName(
                                mTestRule.implementationUnderTest()));
    }
}
