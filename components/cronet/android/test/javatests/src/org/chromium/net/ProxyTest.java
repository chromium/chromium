// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyList;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.test.ServerCertificate;

import java.util.AbstractMap;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Exchanger;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

/** Test Cronet proxy support. */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class ProxyTest {
    private static final int HTTPENGINE_PROXY_API_SDK_EXTENSION = 21;

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private NativeTestServer mNativeTestServer;

    @Before
    public void setUp() throws Exception {
        mNativeTestServer =
                NativeTestServer.createNativeTestServer(mTestRule.getTestFramework().getContext());
    }

    @After
    public void tearDown() throws Exception {
        mNativeTestServer.close();
    }

    @Test
    @SmallTest
    public void testProxy_nullCallback_throws() {
        assertThrows(
                NullPointerException.class,
                () ->
                        Proxy.createHttpProxy(
                                /* scheme= */ Proxy.SCHEME_HTTPS,
                                /* host= */ "this-hostname-does-not-exist.com",
                                /* port= */ 8080,
                                Executors.newSingleThreadExecutor(),
                                /* callback= */ null));
    }

    @Test
    @SmallTest
    public void testProxy_nullHost_throws() {
        Proxy.HttpConnectCallback proxyCallbackMock =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        assertThrows(
                NullPointerException.class,
                () ->
                        Proxy.createHttpProxy(
                                /* scheme= */ Proxy.SCHEME_HTTP,
                                /* host= */ null,
                                /* port= */ 8080,
                                Executors.newSingleThreadExecutor(),
                                /* callback= */ proxyCallbackMock));
    }

    @Test
    @SmallTest
    public void testProxy_nullExecutor_throws() {
        Proxy.HttpConnectCallback proxyCallbackMock =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        assertThrows(
                NullPointerException.class,
                () ->
                        Proxy.createHttpProxy(
                                /* scheme= */ Proxy.SCHEME_HTTP,
                                /* host= */ "this-hostname-does-not-exist.com",
                                /* port= */ 8080,
                                null,
                                /* callback= */ proxyCallbackMock));
    }

    @Test
    @SmallTest
    public void testProxy_invalidScheme_throws() {
        Proxy.HttpConnectCallback proxyCallbackMock =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        Proxy.createHttpProxy(
                                /* scheme= */ -1,
                                /* host= */ "localhost",
                                /* port= */ 8080,
                                Executors.newSingleThreadExecutor(),
                                /* callback= */ proxyCallbackMock));
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        Proxy.createHttpProxy(
                                /* scheme= */ 2,
                                /* host= */ "localhost",
                                /* port= */ 8080,
                                Executors.newSingleThreadExecutor(),
                                /* callback= */ proxyCallbackMock));
    }

    @Test
    @SmallTest
    public void testProxyOptions_nullProxyList_throws() {
        assertThrows(NullPointerException.class, () -> ProxyOptions.fromProxyList(null));
    }

    @Test
    @SmallTest
    public void testProxyOptions_nullProxyIsNotLastElement_throws() {
        assertThrows(
                IllegalArgumentException.class,
                () -> ProxyOptions.fromProxyList(Arrays.asList(null, null)));
        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        Proxy proxy =
                Proxy.createHttpProxy(
                        /* scheme= */ Proxy.SCHEME_HTTPS,
                        /* host= */ "this-hostname-does-not-exist.com",
                        /* port= */ 8080,
                        Executors.newSingleThreadExecutor(),
                        /* callback= */ proxyCallback);
        assertThrows(
                IllegalArgumentException.class,
                () -> ProxyOptions.fromProxyList(Arrays.asList(null, proxy)));
        assertThrows(
                IllegalArgumentException.class,
                () -> ProxyOptions.fromProxyList(Arrays.asList(proxy, null, proxy)));
    }

    @Test
    @SmallTest
    public void testProxyOptions_emptyProxyList_throws() {
        assertThrows(
                IllegalArgumentException.class,
                () -> ProxyOptions.fromProxyList(Collections.emptyList()));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    public void testDirectProxy_requestSucceeds() {
        mNativeTestServer.start();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(Arrays.asList((Proxy) null))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getSuccessURL(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        // This cannot be tested when HttpEngine is used under the hood:
        // android.net.http.UrlResponseInfo does not expose the proxy used for a request.
        if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
            assertThat(callback.getResponseInfoWithChecks()).hasProxyServerThat().isEqualTo(":0");
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testUnreachableProxyWithDirectFallback_requestSucceeds() {
        mNativeTestServer.start();
        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTPS,
                                                                /* host= */ "this-hostname-does-not-exist.com",
                                                                /* port= */ 8080,
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback),
                                                        null))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getSuccessURL(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        // This cannot be tested when HttpEngine is used under the hood:
        // android.net.http.UrlResponseInfo does not expose the proxy used for a request.
        if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
            assertThat(callback.getResponseInfoWithChecks()).hasProxyServerThat().isEqualTo(":0");
        }
        Mockito.verify(proxyCallback, never()).onBeforeRequest(any());
        Mockito.verify(proxyCallback, never()).onResponseReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testUnreachableProxy_requestFails() {
        mNativeTestServer.start();
        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTPS,
                                                                /* host= */ "this-hostname-does-not-exist.com",
                                                                /* port= */ 8080,
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getSuccessURL(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mError).isNotNull();
        Mockito.verify(proxyCallback, never()).onBeforeRequest(any());
        Mockito.verify(proxyCallback, never()).onResponseReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testBrokenProxyWithWorkingFallback_brokenProxyIsDeprioritized() {
        try (NativeTestServer brokenProxyServer = mNativeTestServer;
                NativeTestServer workingProxyServer =
                        NativeTestServer.createNativeTestServer(
                                mTestRule.getTestFramework().getContext());
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            brokenProxyServer.enableConnectProxy(Collections.emptyList());
            brokenProxyServer.start();
            workingProxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            workingProxyServer.start();

            Proxy.HttpConnectCallback brokenProxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(brokenProxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(brokenProxyCallback)
                    .onResponseReceived(any(), anyInt());

            Proxy.HttpConnectCallback workingProxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(workingProxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(workingProxyCallback)
                    .onResponseReceived(any(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ brokenProxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ brokenProxyCallback),
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ workingProxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ workingProxyCallback)))));

            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(brokenProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(brokenProxyCallback, times(1)).onResponseReceived(any(), anyInt());
            Mockito.verify(workingProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(workingProxyCallback, times(1)).onResponseReceived(any(), anyInt());

            callback = new TestUrlRequestCallback();
            urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            // Since `brokenProxy` failed, while `workingProxy` succeeded; Cronet should skip
            // `brokenProxy` and try directly with `workingProxy`. With that in mind, the number of
            // callbacks for `brokenProxyCallback` should not increase.
            Mockito.verify(brokenProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(brokenProxyCallback, times(1)).onResponseReceived(any(), anyInt());
            Mockito.verify(workingProxyCallback, times(2)).onBeforeRequest(any());
            Mockito.verify(workingProxyCallback, times(2)).onResponseReceived(any(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testHttpResource_sendsGetWithFullPathToProxy() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    public NativeTestServer.HttpRequest mReceivedHttpRequest;

                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        assertThat(mReceivedHttpRequest).isNull();
                        mReceivedHttpRequest = httpRequest;
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList("HTTP/1.1 502 Bad Gateway"));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "http://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        // NativeTestServer actually sees "GET http://.../... HTTP/1.1", but sadly it strips the
        // URL and only passes the path section to the callback. See
        // net::test_server::HttpRequestParser::ParseHeaders().
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl()).isEqualTo("/test-path");
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("GET");
        Mockito.verify(proxyCallback, never()).onBeforeRequest(any());
        Mockito.verify(proxyCallback, never()).onResponseReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#when implementation makes use of java.util.Map#computeIfAbsent, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testHttpsResource_sendsConnectWithRelativePathToProxy() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    public NativeTestServer.HttpRequest mReceivedHttpRequest;

                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        assertThat(mReceivedHttpRequest).isNull();
                        mReceivedHttpRequest = httpRequest;
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList("HTTP/1.1 502 Bad Gateway"));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.HttpConnectCallback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeRequest(any());
        Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                .when(proxyCallback)
                .onResponseReceived(any(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl())
                .isEqualTo("test-hostname:443");
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("CONNECT");
        Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
        Mockito.verify(proxyCallback, times(1)).onResponseReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#when implementation makes use of java.util.Map#computeIfAbsent, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_ExtraRequestHeadersAreSent() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    public NativeTestServer.HttpRequest mReceivedHttpRequest;

                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        assertThat(mReceivedHttpRequest).isNull();
                        mReceivedHttpRequest = httpRequest;
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList("HTTP/1.1 502 Bad Gateway", ""));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.HttpConnectCallback.Request request = invocation.getArgument(0);
                            request.proceed(
                                    Arrays.asList(new Pair<>("Authorization", "b3BlbiBzZXNhbWU=")));
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeRequest(any());
        Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                .when(proxyCallback)
                .onResponseReceived(any(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl())
                .isEqualTo("test-hostname:443");
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("CONNECT");
        assertThat(requestHandler.mReceivedHttpRequest.getAllHeaders())
                .contains("\r\nAuthorization: b3BlbiBzZXNhbWU=\r\n");
        Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
        Mockito.verify(proxyCallback, times(1)).onResponseReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#when implementation makes use of java.util.Map#computeIfAbsent, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testProxyAuthChallenge_urlRequestFails() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList(
                                        "HTTP/1.1 407 Proxy Authentication Required",
                                        "Proxy-Authenticate: Basic realm=\"MyRealm1\""));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();

        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.HttpConnectCallback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeRequest(any());
        Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                .when(proxyCallback)
                .onResponseReceived(any(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableHttpCache(
                                    CronetEngine.Builder.HTTP_CACHE_IN_MEMORY, 100 * 1024);
                            builder.setProxyOptions(
                                    ProxyOptions.fromProxyList(
                                            Arrays.asList(
                                                    Proxy.createHttpProxy(
                                                            /* scheme= */ Proxy.SCHEME_HTTP,
                                                            /* host= */ "localhost",
                                                            /* port= */ mNativeTestServer.getPort(),
                                                            Executors.newSingleThreadExecutor(),
                                                            /* callback= */ proxyCallback))));
                        });
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
        Mockito.verify(proxyCallback, times(1)).onResponseReceived(any(), anyInt());
        // TODO(https://crbug.com/447574602): Consider supporting authentication challenges in
        // Cronet. Currently, whenever Cronet encounters a 401/407 we rely on developers to retry
        // the request after adding an Authentication/Proxy-Authentication header. If this turns out
        // to be too cumbersome, we should consider providing an ad-hoc abstraction to handle these
        // (similarly to how //net provides net::HttpAuthController).
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError).isInstanceOf(NetworkException.class);
        NetworkException networkException = (NetworkException) callback.mError;
        assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_OTHER);
        // This cannot be tested when HttpEngine is used under the hood:
        // android.net.http.NetworkException does not expose internal error codes.
        if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
            // This cannot be tested when HttpEngine is used under the hood:
            // android.net.http.NetworkException does not expose internal error codes.
            if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
                assertThat(networkException.getCronetInternalErrorCode())
                        .isEqualTo(NetError.ERR_TUNNEL_CONNECTION_FAILED);
            }
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyResponseFailureIsReported() {
        // See net::test_server::EmbeddedTestServer::EnableConnectProxy: sending requests to
        // destinations other than the one passed will result in 502 responses.
        mNativeTestServer.enableConnectProxy(Arrays.asList("https://not-existing-url.com"));
        mNativeTestServer.start();
        Proxy.HttpConnectCallback proxyCallback =
                Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.HttpConnectCallback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeRequest(any());
        Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                .when(proxyCallback)
                .onResponseReceived(anyList(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mError).isNotNull();
        Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
        // See net::test_server::EmbeddedTestServer::EnableConnectProxy: since we're sending a
        // request to a destination other than https://not-existing-url.com we expect to receive a
        // 502.
        Mockito.verify(proxyCallback, times(1)).onResponseReceived(anyList(), eq(502));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyResponseSuccessIsReported() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proxyCallback)
                    .onResponseReceived(anyList(), anyInt());
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            // The exact values of these headers is not that important. We are just confirming we
            // don't receive the tunnel response headers here.
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersListThat()
                    .containsExactlyElementsIn(
                            Arrays.asList(
                                    new AbstractMap.SimpleImmutableEntry<>(
                                            "Content-Type", "text/plain"),
                                    new AbstractMap.SimpleImmutableEntry<>(
                                            "Access-Control-Allow-Origin", "*"),
                                    new AbstractMap.SimpleImmutableEntry<>(
                                            "header-name", "header-value"),
                                    new AbstractMap.SimpleImmutableEntry<>(
                                            "multi-header-name", "header-value1"),
                                    new AbstractMap.SimpleImmutableEntry<>(
                                            "multi-header-name", "header-value2")));
            // This cannot be tested when HttpEngine is used under the hood:
            // android.net.http.UrlResponseInfo does not expose the proxy used for a request.
            if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
                assertThat(callback.getResponseInfoWithChecks())
                        .hasProxyServerThat()
                        .isEqualTo("localhost:" + proxyServer.getPort());
            }
            assertThat(callback.mResponseAsString).isEqualTo(NativeTestServer.SUCCESS_BODY);
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            ArgumentCaptor<List<Pair<String, String>>> argumentCaptor =
                    ArgumentCaptor.forClass(List.class);
            Mockito.verify(proxyCallback, times(1))
                    .onResponseReceived(argumentCaptor.capture(), eq(200));
            // The exact values of these headers is not that important. We are just confirming we
            // don't receive the actual response headers here.
            assertThat(argumentCaptor.getValue())
                    .containsExactlyElementsIn(
                            Arrays.asList(
                                    new Pair<>("Connection", "close"),
                                    new Pair<>("Content-Length", "0"),
                                    new Pair<>("Content-Type", "")));
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_bidiStream_isSuccessfullyProxied() throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer) {
            assertThat(
                            Http2TestServer.startHttp2TestServer(
                                    mTestRule.getTestFramework().getContext()))
                    .isTrue();
            proxyServer.enableConnectProxy(Arrays.asList(Http2TestServer.getEchoMethodUrl()));
            proxyServer.start();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proxyCallback)
                    .onResponseReceived(anyList(), anyInt());
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
            BidirectionalStream stream =
                    cronetEngine
                            .newBidirectionalStreamBuilder(
                                    Http2TestServer.getEchoMethodUrl(),
                                    callback,
                                    callback.getExecutor())
                            .setHttpMethod("GET")
                            .build();
            stream.start();
            callback.blockForDone();
            assertThat(stream.isDone()).isTrue();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            // The exact values of these headers is not that important. We are just confirming we
            // don't receive the tunnel response headers here.
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersListThat()
                    .containsExactlyElementsIn(
                            Arrays.asList(
                                    new AbstractMap.SimpleImmutableEntry<>(":status", "200")));
            // This cannot be tested when HttpEngine is used under the hood:
            // android.net.http.UrlResponseInfo does not expose the proxy used for a request.
            if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
                // TODO(https://crbug.com/460426595): Change this to check for the correct proxy
                // server value once BidirectionalStream correctly reports proxy servers.
                assertThat(callback.getResponseInfoWithChecks()).hasProxyServerThat().isNull();
            }

            assertThat(callback.mResponseAsString).isEqualTo("GET");
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            ArgumentCaptor<List<Pair<String, String>>> argumentCaptor =
                    ArgumentCaptor.forClass(List.class);
            Mockito.verify(proxyCallback, times(1))
                    .onResponseReceived(argumentCaptor.capture(), eq(200));
            // The exact values of these headers is not that important. We are just confirming we
            // don't receive the actual response headers here.
            assertThat(argumentCaptor.getValue())
                    .containsExactlyElementsIn(
                            Arrays.asList(
                                    new Pair<>("Connection", "close"),
                                    new Pair<>("Content-Length", "0"),
                                    new Pair<>("Content-Type", "")));
        } finally {
            Http2TestServer.shutdownHttp2TestServer();
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyResponse_returningFalseFailsUrlRequest() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_CLOSE)
                    .when(proxyCallback)
                    .onResponseReceived(anyList(), anyInt());
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            // Confirm that Proxy.HttpConnectCallback#onResponseReceived was called reporting a
            // success
            // (status code 200), but that the UrlRequest still failed, since
            // onResponseReceived returned false.
            Mockito.verify(proxyCallback, times(1)).onResponseReceived(anyList(), eq(200));
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) callback.mError;
            assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_OTHER);
            // This cannot be tested when HttpEngine is used under the hood:
            // android.net.http.NetworkException does not expose internal error codes.
            if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
                assertThat(networkException.getCronetInternalErrorCode())
                        .isEqualTo(NetError.ERR_PROXY_DELEGATE_CANCELED_CONNECT_RESPONSE);
            }
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyResponse_throwingFailsUrlRequest() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            doAnswer(
                            invocation -> {
                                throw new RuntimeException("This should fail the UrlRequest");
                            })
                    .when(proxyCallback)
                    .onResponseReceived(anyList(), anyInt());
            Proxy proxy =
                    Proxy.createHttpProxy(
                            /* scheme= */ Proxy.SCHEME_HTTP,
                            /* host= */ "localhost",
                            /* port= */ proxyServer.getPort(),
                            (Runnable r) -> {
                                try {
                                    r.run();
                                } catch (Exception e) {
                                    // Ignore the exception. This is bad practice. We're doing this
                                    // only to confirm the tunnel won't be used as we promise in our
                                    // documentation.
                                }
                            },
                            /* callback= */ proxyCallback);
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(Arrays.asList(proxy))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            // Confirm that Proxy.HttpConnectCallback#onResponseReceived was called reporting a
            // success
            // (status code 200), but that the UrlRequest still failed, since
            // onResponseReceived threw.
            Mockito.verify(proxyCallback, times(1)).onResponseReceived(anyList(), eq(200));
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) callback.mError;
            assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_OTHER);
            // This cannot be tested when HttpEngine is used under the hood:
            // android.net.http.NetworkException does not expose internal error codes.
            if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
                assertThat(networkException.getCronetInternalErrorCode())
                        .isEqualTo(NetError.ERR_PROXY_DELEGATE_CANCELED_CONNECT_RESPONSE);
            }
        }
    }

    @Test
    @LargeTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestHangs_urlRequestTimesOut() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                // We want to hang: ignore the Request object we receive.
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mOnErrorCalled).isTrue();
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) callback.mError;
            assertThat(networkException.getErrorCode())
                    .isAnyOf(
                            NetworkException.ERROR_TIMED_OUT,
                            // Sometimes the device network can disconnect during the test, making
                            // this test flaky. Work around it by accepting ERROR_NETWORK_CHANGED as
                            // a possible failure.
                            NetworkException.ERROR_NETWORK_CHANGED);
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proxyCallback, never()).onResponseReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestProceedAfterEngineShutdown_doesNotCrash()
            throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Exchanger<Proxy.HttpConnectCallback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.HttpConnectCallback.Request>();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.HttpConnectCallback.Request proxyRequest =
                    proxyRequestExchanger.exchange(null)) {
                urlRequest.cancel();
                callback.blockForDone();
                assertThat(callback.mOnCanceledCalled).isTrue();
                assertThat(callback.mError).isNull();
                Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
                Mockito.verify(proxyCallback, never()).onResponseReceived(anyList(), anyInt());

                cronetEngine.shutdown();
                proxyRequest.proceed(Collections.emptyList());
            }
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    @DisabledTest(message = "TODO(https://crbug.com/442024094): Reenable after flakiness is fixed")
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestProceedAfterUrlRequestCancel_doesNotCrash()
            throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Exchanger<Proxy.HttpConnectCallback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.HttpConnectCallback.Request>();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.HttpConnectCallback.Request proxyRequest =
                    proxyRequestExchanger.exchange(null)) {
                Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
                Mockito.verify(proxyCallback, never()).onResponseReceived(anyList(), anyInt());
                urlRequest.cancel();
                proxyRequest.proceed(Collections.emptyList());
                callback.blockForDone();
                assertThat(callback.mOnCanceledCalled).isTrue();
                assertThat(callback.mError).isNull();
            }
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestCloseCalledMultipleTimes_doesNotThrow() throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Exchanger<Proxy.HttpConnectCallback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.HttpConnectCallback.Request>();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            Proxy.HttpConnectCallback.Request proxyRequest = proxyRequestExchanger.exchange(null);
            proxyRequest.close();
            proxyRequest.close();

            callback.blockForDone();
            assertThat(callback.mOnErrorCalled).isTrue();
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) callback.mError;
            assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_OTHER);
            // This cannot be tested when HttpEngine is used under the hood:
            // android.net.http.NetworkException does not expose internal error codes.
            if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
                assertThat(networkException.getCronetInternalErrorCode())
                        .isEqualTo(NetError.ERR_PROXY_DELEGATE_CANCELED_CONNECT_REQUEST);
            }
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proxyCallback, never()).onResponseReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestProceedWithInvalidHeader_throwsButRequestRemainsValid()
            throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Exchanger<Proxy.HttpConnectCallback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.HttpConnectCallback.Request>();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proxyCallback)
                    .onResponseReceived(anyList(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.HttpConnectCallback.Request proxyRequest =
                    proxyRequestExchanger.exchange(null)) {
                assertThrows(
                        IllegalArgumentException.class,
                        () ->
                                proxyRequest.proceed(
                                        Arrays.asList(new Pair<>(":", "valid header value"))));
                assertThrows(
                        IllegalArgumentException.class,
                        () ->
                                proxyRequest.proceed(
                                        Arrays.asList(new Pair<>("Authorization", "\r"))));
                proxyRequest.proceed(Collections.emptyList());
            }

            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proxyCallback, times(1)).onResponseReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestProceedCalledMultipleTimes_throws() throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Exchanger<Proxy.HttpConnectCallback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.HttpConnectCallback.Request>();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proxyCallback)
                    .onResponseReceived(anyList(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.HttpConnectCallback.Request proxyRequest =
                    proxyRequestExchanger.exchange(null)) {
                proxyRequest.proceed(Collections.emptyList());
                assertThrows(
                        IllegalStateException.class,
                        () -> proxyRequest.proceed(Collections.emptyList()));
            }

            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proxyCallback, times(1)).onResponseReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestProceedAfterClose_throws() throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Exchanger<Proxy.HttpConnectCallback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.HttpConnectCallback.Request>();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proxyCallback)
                    .onResponseReceived(anyList(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            Proxy.HttpConnectCallback.Request proxyRequest = proxyRequestExchanger.exchange(null);
            proxyRequest.close();
            assertThrows(
                    IllegalStateException.class,
                    () -> proxyRequest.proceed(Collections.emptyList()));

            callback.blockForDone();
            assertThat(callback.mOnErrorCalled).isTrue();
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) callback.mError;
            assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_OTHER);
            // This cannot be tested when HttpEngine is used under the hood:
            // android.net.http.NetworkException does not expose internal error codes.
            if (mTestRule.implementationUnderTest() != CronetImplementation.AOSP_PLATFORM) {
                assertThat(networkException.getCronetInternalErrorCode())
                        .isEqualTo(NetError.ERR_PROXY_DELEGATE_CANCELED_CONNECT_REQUEST);
            }
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proxyCallback, never()).onResponseReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyRequestCloseAfterProceed_throws() throws Exception {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Exchanger<Proxy.HttpConnectCallback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.HttpConnectCallback.Request>();
            Proxy.HttpConnectCallback proxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proxyCallback)
                    .onResponseReceived(any(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            Proxy.HttpConnectCallback.Request proxyRequest = proxyRequestExchanger.exchange(null);
            proxyRequest.proceed(Collections.emptyList());
            proxyRequest.close();

            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(proxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proxyCallback, times(1)).onResponseReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_fallbackSucceedsAfterProxyRequestCancel_proxyIsDeprioritized() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Proxy.HttpConnectCallback requestCancelProxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.close();
                                return null;
                            })
                    .when(requestCancelProxyCallback)
                    .onBeforeRequest(any());

            Proxy.HttpConnectCallback proceedProxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proceedProxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proceedProxyCallback)
                    .onResponseReceived(any(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ requestCancelProxyCallback),
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proceedProxyCallback)))));

            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(requestCancelProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(requestCancelProxyCallback, never()).onResponseReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proceedProxyCallback, times(1)).onResponseReceived(any(), anyInt());

            callback = new TestUrlRequestCallback();
            urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            // Since `requestCancelProxyCallback` failed the tunnel establishment request, while
            // `proceedProxyCallback` did not; Cronet should skip `requestCancelProxyCallback` and
            // try directly with `proceedProxyCallback`. With that in mind, the number of callbacks
            // for `requestCancelProxyCallback` should not increase.
            // Note: From the perspective of Cronet, the two proxies are different, even though they
            // have the same hostname.
            Mockito.verify(requestCancelProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(requestCancelProxyCallback, never()).onResponseReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(2)).onBeforeRequest(any());
            Mockito.verify(proceedProxyCallback, times(2)).onResponseReceived(any(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_fallbackSucceedsAfterProxyResponseCancel_proxyIsDeprioritized() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();

            Proxy.HttpConnectCallback responseCancelProxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(responseCancelProxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_CLOSE)
                    .when(responseCancelProxyCallback)
                    .onResponseReceived(any(), anyInt());

            Proxy.HttpConnectCallback proceedProxyCallback =
                    Mockito.mock(Proxy.HttpConnectCallback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.HttpConnectCallback.Request request =
                                        invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proceedProxyCallback)
                    .onBeforeRequest(any());
            Mockito.doReturn(Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED)
                    .when(proceedProxyCallback)
                    .onResponseReceived(any(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            ProxyOptions.fromProxyList(
                                                    Arrays.asList(
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ responseCancelProxyCallback),
                                                            Proxy.createHttpProxy(
                                                                    /* scheme= */ Proxy.SCHEME_HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    Executors
                                                                            .newSingleThreadExecutor(),
                                                                    /* callback= */ proceedProxyCallback)))));

            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(responseCancelProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(responseCancelProxyCallback, times(1))
                    .onResponseReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(proceedProxyCallback, times(1)).onResponseReceived(any(), anyInt());

            callback = new TestUrlRequestCallback();
            urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            // Since `responseCancelProxyCallback` failed the tunnel establishment request, while
            // `proceedProxyCallback` did not; Cronet should skip `responseCancelProxyCallback` and
            // try directly with `proceedProxyCallback`. With that in mind, the number of callbacks
            // for `responseCancelProxyCallback` should not increase.
            // Note: From the perspective of Cronet, the two proxies are different, even though they
            // have the same hostname.
            Mockito.verify(responseCancelProxyCallback, times(1)).onBeforeRequest(any());
            Mockito.verify(responseCancelProxyCallback, times(1))
                    .onResponseReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(2)).onBeforeRequest(any());
            Mockito.verify(proceedProxyCallback, times(2)).onResponseReceived(any(), anyInt());
        }
    }

    static class TestProxyCallback extends Proxy.HttpConnectCallback {
        private final AtomicInteger mOnBeforeRequestInvocationTimes = new AtomicInteger(0);
        private final AtomicInteger mOnResponseReceivedInvocationTimes = new AtomicInteger(0);

        public int getonBeforeRequestInvocationTimes() {
            return mOnBeforeRequestInvocationTimes.get();
        }

        public int getonResponseReceivedInvocationTimes() {
            return mOnResponseReceivedInvocationTimes.get();
        }

        @Override
        public void onBeforeRequest(Request request) {
            mOnBeforeRequestInvocationTimes.getAndIncrement();
            request.proceed(Collections.emptyList());
        }

        @Override
        public @Proxy.HttpConnectCallback.OnResponseReceivedAction int onResponseReceived(
                @NonNull List<Pair<String, String>> responseHeaders, int statusCode) {
            mOnResponseReceivedInvocationTimes.getAndIncrement();
            return Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED;
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // This test is written without relying on Mockito. This is necessary because Mockito makes use
    // of Java APIs which are not available on Marshmallow/API level 23. Once support for
    // Marshmallow is dropped, we can move these to Mockito.
    public void testMarshmallowIntegration_proxyCallbacksSuccesfullyCalled() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    public NativeTestServer.HttpRequest mReceivedHttpRequest;

                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        assertThat(mReceivedHttpRequest).isNull();
                        mReceivedHttpRequest = httpRequest;
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList("HTTP/1.1 502 Bad Gateway"));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        TestProxyCallback proxyCallback = new TestProxyCallback();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl())
                .isEqualTo("test-hostname:443");
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("CONNECT");
        assertThat(proxyCallback.getonResponseReceivedInvocationTimes()).isEqualTo(1);
        assertThat(proxyCallback.getonBeforeRequestInvocationTimes()).isEqualTo(1);
    }

    static final class NoOpProxyCallbackRequest extends Proxy.HttpConnectCallback.Request {
        @Override
        public void proceed(List<Pair<String, String>> extraHeaders) {}

        @Override
        public void close() {}
    }

    static class CloseDuringRequestProxyCallback extends TestProxyCallback {
        @Override
        public void onBeforeRequest(Request request) {
            super.onBeforeRequest(new NoOpProxyCallbackRequest());
            request.close();
        }

        @Override
        public @Proxy.HttpConnectCallback.OnResponseReceivedAction int onResponseReceived(
                @NonNull List<Pair<String, String>> responseHeaders, int statusCode) {
            super.onResponseReceived(responseHeaders, statusCode);
            return Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED;
        }
    }

    static class CloseDuringResponseProxyCallback extends TestProxyCallback {
        @Override
        public @Proxy.HttpConnectCallback.OnResponseReceivedAction int onResponseReceived(
                @NonNull List<Pair<String, String>> responseHeaders, int statusCode) {
            super.onResponseReceived(responseHeaders, statusCode);
            return Proxy.HttpConnectCallback.RESPONSE_ACTION_CLOSE;
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // This test is written without relying on Mockito. This is necessary because Mockito makes use
    // of Java APIs which are not available on Marshmallow/API level 23. Once support for
    // Marshmallow is dropped, we can move these to Mockito.
    public void testMarshmallowIntegration_multipleProxiesInList() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    public NativeTestServer.HttpRequest mReceivedHttpRequest;

                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        assertThat(mReceivedHttpRequest).isNull();
                        mReceivedHttpRequest = httpRequest;
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList("HTTP/1.1 502 Bad Gateway", ""));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        TestProxyCallback closeDuringRequestProxyCallback = new CloseDuringRequestProxyCallback();
        TestProxyCallback closeDuringResponseProxyCallback = new CloseDuringResponseProxyCallback();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ closeDuringRequestProxyCallback),
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ closeDuringResponseProxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl())
                .isEqualTo("test-hostname:443");
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("CONNECT");
        assertThat(closeDuringRequestProxyCallback.getonBeforeRequestInvocationTimes())
                .isEqualTo(1);
        assertThat(closeDuringRequestProxyCallback.getonResponseReceivedInvocationTimes())
                .isEqualTo(0);
        assertThat(closeDuringResponseProxyCallback.getonResponseReceivedInvocationTimes())
                .isEqualTo(1);
        assertThat(closeDuringResponseProxyCallback.getonBeforeRequestInvocationTimes())
                .isEqualTo(1);
    }

    static class AddExtraRequestHeadersProxyCallback extends TestProxyCallback {
        @Override
        public void onBeforeRequest(Request request) {
            super.onBeforeRequest(new NoOpProxyCallbackRequest());
            request.proceed(
                    Arrays.asList(
                            new Pair<>("Authorization", "b3BlbiBzZXNhbWU="),
                            new Pair<>("CustomHeader", "CustomValue123")));
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.",
            requiredSdkExtensionForPlatform = HTTPENGINE_PROXY_API_SDK_EXTENSION)
    // This test is written without relying on Mockito. This is necessary because Mockito makes use
    // of Java APIs which are not available on Marshmallow/API level 23. Once support for
    // Marshmallow is dropped, we can move these to Mockito.
    public void testMarshmallowIntegration_multipleExtraHeaders() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    public NativeTestServer.HttpRequest mReceivedHttpRequest;

                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        assertThat(mReceivedHttpRequest).isNull();
                        mReceivedHttpRequest = httpRequest;
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList("HTTP/1.1 502 Bad Gateway", ""));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.HttpConnectCallback proxyCallback = new AddExtraRequestHeadersProxyCallback();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        ProxyOptions.fromProxyList(
                                                Arrays.asList(
                                                        Proxy.createHttpProxy(
                                                                /* scheme= */ Proxy.SCHEME_HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                Executors.newSingleThreadExecutor(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getAllHeaders())
                .contains("\r\nAuthorization: b3BlbiBzZXNhbWU=\r\n");
        assertThat(requestHandler.mReceivedHttpRequest.getAllHeaders())
                .contains("\r\nCustomHeader: CustomValue123\r\n");
    }
}
