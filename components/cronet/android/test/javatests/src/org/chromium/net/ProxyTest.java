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
import org.chromium.net.CronetTestRule.CronetImplementation;
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

/** Test Cronet proxy support. */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class ProxyTest {
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
        Proxy.Callback proxyCallbackMock = Mockito.mock(Proxy.Callback.class);
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
        Proxy.Callback proxyCallbackMock = Mockito.mock(Proxy.Callback.class);
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
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
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
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
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
    @DisabledTest(message = "We need the ability to spawn multiple NativeTestServer to test this.")
    public void testUnreachableProxy_isDeprioritized() {
        mNativeTestServer.start();
        Proxy.Callback unreachableProxyCallback = Mockito.mock(Proxy.Callback.class);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.close();
                            return null;
                        })
                .when(unreachableProxyCallback)
                .onBeforeTunnelRequest(any());

        Proxy.Callback reachableProxyCallback = Mockito.mock(Proxy.Callback.class);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(reachableProxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.when(reachableProxyCallback.onTunnelHeadersReceived(any(), anyInt()))
                .thenReturn(true);
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
                                                                /* callback= */ unreachableProxyCallback),
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                /* callback= */ reachableProxyCallback)))));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        Mockito.verify(unreachableProxyCallback, times(1)).onBeforeTunnelRequest(any());
        Mockito.verify(unreachableProxyCallback, never()).onTunnelHeadersReceived(any(), anyInt());
        Mockito.verify(reachableProxyCallback, times(1)).onBeforeTunnelRequest(any());
        Mockito.verify(reachableProxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());

        callback = new TestUrlRequestCallback();
        urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        Mockito.verify(unreachableProxyCallback, times(1)).onBeforeTunnelRequest(any());
        Mockito.verify(unreachableProxyCallback, never()).onTunnelHeadersReceived(any(), anyInt());
        Mockito.verify(reachableProxyCallback, times(2)).onBeforeTunnelRequest(any());
        Mockito.verify(reachableProxyCallback, times(2)).onTunnelHeadersReceived(any(), anyInt());
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.when(proxyCallback.onTunnelHeadersReceived(anyList(), anyInt())).thenReturn(true);
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(
                                    Arrays.asList(
                                            new AbstractMap.SimpleEntry<>(
                                                    "Authorization", "b3BlbiBzZXNhbWU=")));
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.when(proxyCallback.onTunnelHeadersReceived(any(), anyInt())).thenReturn(true);
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
    // Mockito#verify implementations makes use of java.util.stream.Stream, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_responseFailureIsReported() {
        // See net::test_server::EmbeddedTestServer::EnableConnectProxy: sending requests to
        // destinations other than the one passed will result in 502 responses.
        mNativeTestServer.enableConnectProxy(Arrays.asList("https://not-existing-url.com"));
        mNativeTestServer.start();
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
        doAnswer(
                        invocation -> {
                            Proxy.Callback.Request request = invocation.getArgument(0);
                            request.proceed(Collections.emptyList());
                            return null;
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest(any());
        Mockito.when(proxyCallback.onTunnelHeadersReceived(any(), anyInt())).thenReturn(true);
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
    public void testCallback_responseSuccessIsReported() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
            doAnswer(
                            invocation -> {
                                Proxy.Callback.Request request = invocation.getArgument(0);
                                request.proceed(Collections.emptyList());
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.when(proxyCallback.onTunnelHeadersReceived(anyList(), anyInt()))
                    .thenReturn(true);
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
    @LargeTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM, CronetImplementation.FALLBACK},
            reason =
                    "This feature flag has not reached platform Cronet yet. Fallback provides no"
                            + " ProxyOptions support.")
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_requestHangs_urlRequestTimesOut() {
        try (NativeTestServer proxyServer = mNativeTestServer;
                NativeTestServer originServer =
                        NativeTestServer.createNativeTestServerWithHTTPS(
                                mTestRule.getTestFramework().getContext(),
                                ServerCertificate.CERT_OK)) {
            originServer.start();
            proxyServer.enableConnectProxy(Arrays.asList(originServer.getSuccessURL()));
            proxyServer.start();
            // We want to hang: do not mock the callback methods.
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
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
            assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_TIMED_OUT);
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
    public void testCallback_requestProceedAfterEngineShutdown_doesNotCrash() throws Exception {
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
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
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
    // Mockito fails on Marshmallow with NoClassDefFoundError:
    // org.mockito.internal.invocation.TypeSafeMatching$$ExternalSyntheticLambda0
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testCallback_requestProceedAfterUrlRequestclose_doesNothing() throws Exception {
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
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
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
                urlRequest.cancel();
                proxyRequest.proceed(Collections.emptyList());
            }

            callback.blockForDone();
            assertThat(callback.mOnCanceledCalled).isTrue();
            assertThat(callback.mError).isNull();
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
    public void testCallback_requestcloseCalledMultipleTimes_doesNotThrow() throws Exception {
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
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
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
    public void testCallback_requestProceedWithInvalidHeader_throwsButRequestRemainsValid()
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
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.when(proxyCallback.onTunnelHeadersReceived(anyList(), anyInt()))
                    .thenReturn(true);

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
                                        Arrays.asList(
                                                new AbstractMap.SimpleEntry<>(
                                                        ":", "valid header value"))));
                assertThrows(
                        IllegalArgumentException.class,
                        () ->
                                proxyRequest.proceed(
                                        Arrays.asList(
                                                new AbstractMap.SimpleEntry<>(
                                                        "Authorization", "\r"))));
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
    public void testCallback_requestProceedCalledMultipleTimes_throws() throws Exception {
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
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.when(proxyCallback.onTunnelHeadersReceived(anyList(), anyInt()))
                    .thenReturn(true);

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
    public void testCallback_requestProceedAfterClose_throws() throws Exception {
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
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.when(proxyCallback.onTunnelHeadersReceived(anyList(), anyInt()))
                    .thenReturn(true);

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
    public void testCallback_requestCloseAfterProceed_throws() throws Exception {
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
            Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
            doAnswer(
                            invocation -> {
                                proxyRequestExchanger.exchange(invocation.getArgument(0));
                                return null;
                            })
                    .when(proxyCallback)
                    .onBeforeTunnelRequest(any());
            Mockito.when(proxyCallback.onTunnelHeadersReceived(anyList(), anyInt()))
                    .thenReturn(true);

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
                        return new NativeTestServer.RawHttpResponse("", "");
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
        public void proceed(List<Map.Entry<String, String>> extraHeaders) {}

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
                        return new NativeTestServer.RawHttpResponse("", "");
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
                            new AbstractMap.SimpleEntry<>("Authorization", "b3BlbiBzZXNhbWU="),
                            new AbstractMap.SimpleEntry<>("CustomHeader", "CustomValue123")));
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
                        return new NativeTestServer.RawHttpResponse("", "");
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
