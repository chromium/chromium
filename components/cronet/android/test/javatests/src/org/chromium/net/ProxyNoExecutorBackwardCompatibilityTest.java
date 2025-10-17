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
import java.util.Map;
import java.util.concurrent.Exchanger;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test Cronet proxy support, exactly like ProxyTest, but using the deprecated APIs that don't
 * specify an Executor for Proxy's constructor.
 */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
// Prevent the linter from complaining about referring to Proxy.HttpConnectCallback.Request as
// Proxy.Callback.Request.
@SuppressWarnings("NonCanonicalType")
public class ProxyNoExecutorBackwardCompatibilityTest {
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
                        new Proxy(
                                /* scheme= */ Proxy.HTTPS,
                                /* host= */ "this-hostname-does-not-exist.com",
                                /* port= */ 8080,
                                /* callback= */ null));
    }

    @Test
    @SmallTest
    public void testProxy_nullHost_throws() {
        Proxy.Callback proxyCallbackMock =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        assertThrows(
                NullPointerException.class,
                () ->
                        new Proxy(
                                /* scheme= */ Proxy.HTTP,
                                /* host= */ null,
                                /* port= */ 8080,
                                /* callback= */ proxyCallbackMock));
    }

    @Test
    @SmallTest
    public void testProxy_invalidScheme_throws() {
        Proxy.Callback proxyCallbackMock =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        new Proxy(
                                /* scheme= */ -1,
                                /* host= */ "localhost",
                                /* port= */ 8080,
                                /* callback= */ proxyCallbackMock));
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        new Proxy(
                                /* scheme= */ 2,
                                /* host= */ "localhost",
                                /* port= */ 8080,
                                /* callback= */ proxyCallbackMock));
    }

    @Test
    @SmallTest
    public void testProxyOptions_nullProxyList_throws() {
        assertThrows(NullPointerException.class, () -> new ProxyOptions(null));
    }

    @Test
    @SmallTest
    public void testProxyOptions_nullProxyIsNotLastElement_throws() {
        assertThrows(
                IllegalArgumentException.class, () -> new ProxyOptions(Arrays.asList(null, null)));
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        Proxy proxy =
                new Proxy(
                        /* scheme= */ Proxy.HTTPS,
                        /* host= */ "this-hostname-does-not-exist.com",
                        /* port= */ 8080,
                        /* callback= */ proxyCallback);
        assertThrows(
                IllegalArgumentException.class, () -> new ProxyOptions(Arrays.asList(null, proxy)));
        assertThrows(
                IllegalArgumentException.class,
                () -> new ProxyOptions(Arrays.asList(proxy, null, proxy)));
    }

    @Test
    @SmallTest
    public void testProxyOptions_emptyProxyList_throws() {
        assertThrows(
                IllegalArgumentException.class, () -> new ProxyOptions(Collections.emptyList()));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
    public void testDirectProxy_requestSucceeds() {
        mNativeTestServer.start();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(Arrays.asList((Proxy) null))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getSuccessURL(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.getResponseInfoWithChecks()).hasProxyServerThat().isEqualTo(":0");
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testUnreachableProxyWithDirectFallback_requestSucceeds() {
        mNativeTestServer.start();
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTPS,
                                                                /* host= */ "this-hostname-does-not-exist.com",
                                                                /* port= */ 8080,
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
        assertThat(callback.getResponseInfoWithChecks()).hasProxyServerThat().isEqualTo(":0");
        Mockito.verify(proxyCallback, never()).onBeforeTunnelRequest(any());
        Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testUnreachableProxy_requestFails() {
        mNativeTestServer.start();
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTPS,
                                                                /* host= */ "this-hostname-does-not-exist.com",
                                                                /* port= */ 8080,
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        mNativeTestServer.getSuccessURL(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mError).isNotNull();
        Mockito.verify(proxyCallback, never()).onBeforeTunnelRequest(any());
        Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
    @DisabledTest(
            message =
                    "TODO(https://crbug.com/440096216): Make Cronet fallback for"
                            + " ERR_TUNNEL_CONNECTION_FAILED")
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

            Proxy.Callback brokenProxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(brokenProxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true)
                    .when(brokenProxyCallback)
                    .onTunnelHeadersReceived(any(), anyInt());

            Proxy.Callback workingProxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(workingProxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true)
                    .when(workingProxyCallback)
                    .onTunnelHeadersReceived(any(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ brokenProxyServer
                                                                            .getPort(),
                                                                    /* callback= */ brokenProxyCallback),
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ workingProxyServer
                                                                            .getPort(),
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
            Mockito.verify(brokenProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(brokenProxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());
            Mockito.verify(workingProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(workingProxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());

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
            Mockito.verify(brokenProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(brokenProxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());
            Mockito.verify(workingProxyCallback, times(2)).onBeforeTunnelRequest(any());
            Mockito.verify(workingProxyCallback, times(2)).onTunnelHeadersReceived(any(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
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
        Mockito.verify(proxyCallback, never()).onBeforeTunnelRequest(any());
        Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(anyList(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
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
        Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
        Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(
                                    Arrays.asList(new Pair<>("Authorization", "b3BlbiBzZXNhbWU=")));
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(any(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
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
        Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
        Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(any(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableHttpCache(
                                    CronetEngine.Builder.HTTP_CACHE_IN_MEMORY, 100 * 1024);
                            builder.setProxyOptions(
                                    new ProxyOptions(
                                            Arrays.asList(
                                                    new Proxy(
                                                            /* scheme= */ Proxy.HTTP,
                                                            /* host= */ "localhost",
                                                            /* port= */ mNativeTestServer.getPort(),
                                                            /* callback= */ proxyCallback))));
                        });
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
        Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());
        // TODO(https://crbug.com/447574602): Consider supporting authentication challenges in
        // Cronet. Currently, whenever Cronet encounters a 401/407 we rely on developers to retry
        // the request after adding an Authentication/Proxy-Authentication header. If this turns out
        // to be too cumbersome, we should consider providing an ad-hoc abstraction to handle these
        // (similarly to how //net provides net::HttpAuthController).
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError).isInstanceOf(NetworkException.class);
        NetworkException networkException = (NetworkException) callback.mError;
        assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_OTHER);
        assertThat(networkException.getCronetInternalErrorCode())
                .isEqualTo(NetError.ERR_TUNNEL_CONNECTION_FAILED);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_proxyResponseFailureIsReported() {
        // See net::test_server::EmbeddedTestServer::EnableConnectProxy: sending requests to
        // destinations other than the one passed will result in 502 responses.
        mNativeTestServer.enableConnectProxy(Arrays.asList("https://not-existing-url.com"));
        mNativeTestServer.start();
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(any(), anyInt());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                /* callback= */ proxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mError).isNotNull();
        Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
        // See net::test_server::EmbeddedTestServer::EnableConnectProxy: since we're sending a
        // request to a destination other than https://not-existing-url.com we expect to receive a
        // 502.
        Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(anyList(), eq(502));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(anyList(), anyInt());
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
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
            // don't receive the tunnel response heeaders here.
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersListThat()
                    .containsExactlyElementsIn(
                            Arrays.asList(
                                    new AbstractMap.SimpleEntry<>("Content-Type", "text/plain"),
                                    new AbstractMap.SimpleEntry<>(
                                            "Access-Control-Allow-Origin", "*"),
                                    new AbstractMap.SimpleEntry<>("header-name", "header-value"),
                                    new AbstractMap.SimpleEntry<>(
                                            "multi-header-name", "header-value1"),
                                    new AbstractMap.SimpleEntry<>(
                                            "multi-header-name", "header-value2")));
            assertThat(callback.getResponseInfoWithChecks())
                    .hasProxyServerThat()
                    .isEqualTo("localhost:" + proxyServer.getPort());
            assertThat(callback.mResponseAsString).isEqualTo(NativeTestServer.SUCCESS_BODY);
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            ArgumentCaptor<List<Map.Entry<String, String>>> argumentCaptor =
                    ArgumentCaptor.forClass(List.class);
            Mockito.verify(proxyCallback, times(1))
                    .onTunnelHeadersReceived(argumentCaptor.capture(), eq(200));
            // The exact values of these headers is not that important. We are just confirming we
            // don't receive the actual response headers here.
            assertThat(argumentCaptor.getValue())
                    .containsExactlyElementsIn(
                            Arrays.asList(
                                    new AbstractMap.SimpleEntry<>("Connection", "close"),
                                    new AbstractMap.SimpleEntry<>("Content-Length", "0"),
                                    new AbstractMap.SimpleEntry<>("Content-Type", "")));
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(false)
                    .when(proxyCallback)
                    .onTunnelHeadersReceived(anyList(), anyInt());
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            // Confirm that Proxy.Callback#onTunnelHeadersReceived was called reporting a success
            // (status code 200), but that the UrlRequest still failed, since
            // onTunnelHeadersReceived returned false.
            Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(anyList(), eq(200));
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            NetworkException networkException = (NetworkException) callback.mError;
            assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_OTHER);
            assertThat(networkException.getCronetInternalErrorCode())
                    .isEqualTo(NetError.ERR_PROXY_DELEGATE_CANCELED_CONNECT_RESPONSE);
        }
    }

    @Test
    @LargeTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                // We want to hang: ignore the Request object we receive.
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
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
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

            Exchanger<Proxy.Callback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.Callback.Request>();
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.Callback.Request proxyRequest = proxyRequestExchanger.exchange(null)) {
                urlRequest.cancel();
                callback.blockForDone();
                assertThat(callback.mOnCanceledCalled).isTrue();
                assertThat(callback.mError).isNull();
                Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
                Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(anyList(), anyInt());

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
                            + " ProxyOptions support.")
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

            Exchanger<Proxy.Callback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.Callback.Request>();
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.Callback.Request proxyRequest = proxyRequestExchanger.exchange(null)) {
                Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
                Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(anyList(), anyInt());
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
                            + " ProxyOptions support.")
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

            Exchanger<Proxy.Callback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.Callback.Request>();
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            Proxy.Callback.Request proxyRequest = proxyRequestExchanger.exchange(null);
            proxyRequest.close();
            proxyRequest.close();

            callback.blockForDone();
            assertThat(callback.mOnErrorCalled).isTrue();
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

            Exchanger<Proxy.Callback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.Callback.Request>();
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(anyList(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.Callback.Request proxyRequest = proxyRequestExchanger.exchange(null)) {
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
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

            Exchanger<Proxy.Callback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.Callback.Request>();
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(anyList(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            try (Proxy.Callback.Request proxyRequest = proxyRequestExchanger.exchange(null)) {
                proxyRequest.proceed(Collections.emptyList());
                assertThrows(
                        IllegalStateException.class,
                        () -> proxyRequest.proceed(Collections.emptyList()));
            }

            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

            Exchanger<Proxy.Callback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.Callback.Request>();
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(anyList(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            Proxy.Callback.Request proxyRequest = proxyRequestExchanger.exchange(null);
            proxyRequest.close();
            assertThrows(
                    IllegalStateException.class,
                    () -> proxyRequest.proceed(Collections.emptyList()));

            callback.blockForDone();
            assertThat(callback.mOnErrorCalled).isTrue();
            assertThat(callback.mError).isNotNull();
            assertThat(callback.mError).isInstanceOf(NetworkException.class);
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proxyCallback, never()).onTunnelHeadersReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

            Exchanger<Proxy.Callback.Request> proxyRequestExchanger =
                    new Exchanger<Proxy.Callback.Request>();
            Proxy.Callback proxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true).when(proxyCallback).onTunnelHeadersReceived(anyList(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ proxyCallback)))));
            ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(
                            originServer.getSuccessURL(), callback, callback.getExecutor());
            final UrlRequest urlRequest = urlRequestBuilder.build();
            urlRequest.start();

            Proxy.Callback.Request proxyRequest = proxyRequestExchanger.exchange(null);
            proxyRequest.proceed(Collections.emptyList());
            proxyRequest.close();

            callback.blockForDone();
            assertThat(callback.mError).isNull();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(anyList(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

            Proxy.Callback requestCancelProxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.close();
                                return null;
                            })
                    .when(requestCancelProxyCallback)
                    .onBeforeTunnelRequest(any());

            Proxy.Callback proceedProxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proceedProxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true)
                    .when(proceedProxyCallback)
                    .onTunnelHeadersReceived(any(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ requestCancelProxyCallback),
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
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
            Mockito.verify(requestCancelProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(requestCancelProxyCallback, never())
                    .onTunnelHeadersReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proceedProxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());

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
            Mockito.verify(requestCancelProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(requestCancelProxyCallback, never())
                    .onTunnelHeadersReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(2)).onBeforeTunnelRequest(any());
            Mockito.verify(proceedProxyCallback, times(2)).onTunnelHeadersReceived(any(), anyInt());
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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

            Proxy.Callback responseCancelProxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(responseCancelProxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(false)
                    .when(responseCancelProxyCallback)
                    .onTunnelHeadersReceived(any(), anyInt());

            Proxy.Callback proceedProxyCallback =
                    Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proceedProxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.doReturn(true)
                    .when(proceedProxyCallback)
                    .onTunnelHeadersReceived(any(), anyInt());

            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    builder.setProxyOptions(
                                            new ProxyOptions(
                                                    Arrays.asList(
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
                                                                    /* callback= */ responseCancelProxyCallback),
                                                            new Proxy(
                                                                    /* scheme= */ Proxy.HTTP,
                                                                    /* host= */ "localhost",
                                                                    /* port= */ proxyServer
                                                                            .getPort(),
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
            Mockito.verify(responseCancelProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(responseCancelProxyCallback, times(1))
                    .onTunnelHeadersReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(proceedProxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());

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
            Mockito.verify(responseCancelProxyCallback, times(1)).onBeforeTunnelRequest(any());
            Mockito.verify(responseCancelProxyCallback, times(1))
                    .onTunnelHeadersReceived(any(), anyInt());
            Mockito.verify(proceedProxyCallback, times(2)).onBeforeTunnelRequest(any());
            Mockito.verify(proceedProxyCallback, times(2)).onTunnelHeadersReceived(any(), anyInt());
        }
    }

    static class TestProxyCallback extends Proxy.Callback {
        private final AtomicInteger mOnBeforeTunnelRequestInvocationTimes = new AtomicInteger(0);
        private final AtomicInteger mOnTunnelHeadersReceivedInvocationTimes = new AtomicInteger(0);

        public int getOnBeforeTunnelRequestInvocationTimes() {
            return mOnBeforeTunnelRequestInvocationTimes.get();
        }

        public int getOnTunnelHeadersReceivedInvocationTimes() {
            return mOnTunnelHeadersReceivedInvocationTimes.get();
        }

        @Override
        public void onBeforeTunnelRequest(Request request) {
            mOnBeforeTunnelRequestInvocationTimes.getAndIncrement();
            request.proceed(Collections.emptyList());
        }

        @Override
        public boolean onTunnelHeadersReceived(
                @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode) {
            mOnTunnelHeadersReceivedInvocationTimes.getAndIncrement();
            return true;
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
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
        assertThat(proxyCallback.getOnTunnelHeadersReceivedInvocationTimes()).isEqualTo(1);
        assertThat(proxyCallback.getOnBeforeTunnelRequestInvocationTimes()).isEqualTo(1);
    }

    static final class NoOpProxyCallbackRequest extends Proxy.Callback.Request {
        @Override
        public void proceed(List<Pair<String, String>> extraHeaders) {}

        @Override
        public void close() {}
    }

    static class CloseDuringRequestProxyCallback extends TestProxyCallback {
        @Override
        public void onBeforeTunnelRequest(Request request) {
            super.onBeforeTunnelRequest(new NoOpProxyCallbackRequest());
            request.close();
        }

        @Override
        public boolean onTunnelHeadersReceived(
                @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode) {
            super.onTunnelHeadersReceived(responseHeaders, statusCode);
            return true;
        }
    }

    static class CloseDuringResponseProxyCallback extends TestProxyCallback {
        @Override
        public boolean onTunnelHeadersReceived(
                @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode) {
            super.onTunnelHeadersReceived(responseHeaders, statusCode);
            return false;
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
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
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                /* callback= */ closeDuringRequestProxyCallback),
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
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
        assertThat(closeDuringRequestProxyCallback.getOnBeforeTunnelRequestInvocationTimes())
                .isEqualTo(1);
        assertThat(closeDuringRequestProxyCallback.getOnTunnelHeadersReceivedInvocationTimes())
                .isEqualTo(0);
        assertThat(closeDuringResponseProxyCallback.getOnTunnelHeadersReceivedInvocationTimes())
                .isEqualTo(1);
        assertThat(closeDuringResponseProxyCallback.getOnBeforeTunnelRequestInvocationTimes())
                .isEqualTo(1);
    }

    static class AddExtraRequestHeadersProxyCallback extends TestProxyCallback {
        @Override
        public void onBeforeTunnelRequest(Request request) {
            super.onBeforeTunnelRequest(new NoOpProxyCallbackRequest());
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
                            + " ProxyOptions support.")
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
        Proxy.Callback proxyCallback = new AddExtraRequestHeadersProxyCallback();
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) ->
                                builder.setProxyOptions(
                                        new ProxyOptions(
                                                Arrays.asList(
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
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
