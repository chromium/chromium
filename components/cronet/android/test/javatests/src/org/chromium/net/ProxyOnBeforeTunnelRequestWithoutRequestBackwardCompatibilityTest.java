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
import androidx.annotation.Nullable;
import androidx.test.ext.junit.runners.AndroidJUnit4;
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
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test Cronet proxy support, exactly like ProxyTest, but using the deprecated APIs that don't pass
 * Proxy.Callback.Request to Proxy.Callback#onBeforeTunnelRequest.
 */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class ProxyOnBeforeTunnelRequestWithoutRequestBackwardCompatibilityTest {
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
        // Stops Mockito from implementing the new onBeforeTunnelRequest(Request) variant. Without
        // this, we would be overriding the fallback behavior that calls into
        // onBeforeTunnelRequest() from onBeforeTunnelRequest(Request).
        Mockito.doCallRealMethod().when(proxyCallbackMock).onBeforeTunnelRequest(any());
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
        // Stops Mockito from implementing the new onBeforeTunnelRequest(Request) variant. Without
        // this, we would be overriding the fallback behavior that calls into
        // onBeforeTunnelRequest() from onBeforeTunnelRequest(Request).
        Mockito.doCallRealMethod().when(proxyCallbackMock).onBeforeTunnelRequest(any());
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
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        // Stops Mockito from implementing the new onBeforeTunnelRequest(Request) variant. Without
        // this, we would be overriding the fallback behavior that calls into
        // onBeforeTunnelRequest() from onBeforeTunnelRequest(Request).
        Mockito.doCallRealMethod().when(proxyCallback).onBeforeTunnelRequest(any());
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
        Mockito.verify(proxyCallback, never()).onBeforeTunnelRequest();
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
        // Stops Mockito from implementing the new onBeforeTunnelRequest(Request) variant. Without
        // this, we would be overriding the fallback behavior that calls into
        // onBeforeTunnelRequest() from onBeforeTunnelRequest(Request).
        Mockito.doCallRealMethod().when(proxyCallback).onBeforeTunnelRequest(any());
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
        Mockito.verify(proxyCallback, never()).onBeforeTunnelRequest();
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
        Proxy.Callback unreachableProxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        Mockito.doReturn(null).when(unreachableProxyCallback).onBeforeTunnelRequest();

        Proxy.Callback reachableProxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        Mockito.doReturn(Collections.emptyList())
                .when(reachableProxyCallback)
                .onBeforeTunnelRequest();
        Mockito.doReturn(true)
                .when(reachableProxyCallback)
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
        Mockito.verify(unreachableProxyCallback, times(1)).onBeforeTunnelRequest();
        Mockito.verify(unreachableProxyCallback, never()).onTunnelHeadersReceived(any(), anyInt());
        Mockito.verify(reachableProxyCallback, times(1)).onBeforeTunnelRequest();
        Mockito.verify(reachableProxyCallback, times(1)).onTunnelHeadersReceived(any(), anyInt());

        callback = new TestUrlRequestCallback();
        urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        "https://test-hostname/test-path", callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        Mockito.verify(unreachableProxyCallback, times(1)).onBeforeTunnelRequest();
        Mockito.verify(unreachableProxyCallback, never()).onTunnelHeadersReceived(any(), anyInt());
        Mockito.verify(reachableProxyCallback, times(2)).onBeforeTunnelRequest();
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
                        return NativeTestServer.RawHttpResponse.createFromHeaders(
                                Arrays.asList("HTTP/1.1 502 Bad Gateway"));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        // Stops Mockito from implementing the new onBeforeTunnelRequest(Request) variant. Without
        // this, we would be overriding the fallback behavior that calls into
        // onBeforeTunnelRequest() from onBeforeTunnelRequest(Request).
        Mockito.doCallRealMethod().when(proxyCallback).onBeforeTunnelRequest(any());
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
        Mockito.verify(proxyCallback, never()).onBeforeTunnelRequest();
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
        // Stops Mockito from implementing the new onBeforeTunnelRequest(Request) variant. Without
        // this, we would be overriding the fallback behavior that calls into
        // onBeforeTunnelRequest() from onBeforeTunnelRequest(Request).
        Mockito.doCallRealMethod().when(proxyCallback).onBeforeTunnelRequest(any());
        Mockito.doReturn(Collections.emptyList()).when(proxyCallback).onBeforeTunnelRequest();
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
        Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest();
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
                            return Collections.emptyList();
                        })
                .when(proxyCallback)
                .onBeforeTunnelRequest();
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
    // Mockito#when implementation makes use of java.util.Map#computeIfAbsent, which is available
    // starting from Nougat/API level 24.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testExtraRequestHeadersAreSent() {
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
        Mockito.doReturn(
                        Arrays.asList(
                                new AbstractMap.SimpleEntry<>("Authorization", "b3BlbiBzZXNhbWU=")))
                .when(proxyCallback)
                .onBeforeTunnelRequest();
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
        Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest();
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
    public void testConnectResponse_failureIsReported() {
        // See net::test_server::EmbeddedTestServer::EnableConnectProxy: sending requests to
        // destinations other than the one passed will result in 502 responses.
        mNativeTestServer.enableConnectProxy(Arrays.asList("https://not-existing-url.com"));
        mNativeTestServer.start();
        Proxy.Callback proxyCallback =
                Mockito.mock(Proxy.Callback.class, Mockito.CALLS_REAL_METHODS);
        Mockito.doReturn(Collections.emptyList()).when(proxyCallback).onBeforeTunnelRequest();
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
        Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest();
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
    public void testConnectResponse_successIsReported() {
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
            Mockito.doReturn(Collections.emptyList()).when(proxyCallback).onBeforeTunnelRequest();
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
            Mockito.verify(proxyCallback, times(1)).onBeforeTunnelRequest();
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
        public @Nullable List<Map.Entry<String, String>> onBeforeTunnelRequest() {
            mOnBeforeTunnelRequestInvocationTimes.getAndIncrement();
            return Collections.emptyList();
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

    static class CancelDuringRequestProxyCallback extends TestProxyCallback {
        @Override
        public @Nullable List<Map.Entry<String, String>> onBeforeTunnelRequest() {
            super.onBeforeTunnelRequest();
            return null;
        }

        @Override
        public boolean onTunnelHeadersReceived(
                @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode) {
            super.onTunnelHeadersReceived(responseHeaders, statusCode);
            return true;
        }
    }

    static class CancelDuringResponseProxyCallback extends TestProxyCallback {
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
                                Arrays.asList("HTTP/1.1 502 Bad Gateway"));
                    }
                };
        mNativeTestServer.registerRequestHandler(requestHandler);
        mNativeTestServer.start();
        TestProxyCallback cancelDuringRequestProxyCallback = new CancelDuringRequestProxyCallback();
        TestProxyCallback cancelDuringResponseProxyCallback =
                new CancelDuringResponseProxyCallback();
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
                                                                /* callback= */ cancelDuringRequestProxyCallback),
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ mNativeTestServer
                                                                        .getPort(),
                                                                /* callback= */ cancelDuringResponseProxyCallback)))));
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
        assertThat(cancelDuringRequestProxyCallback.getOnBeforeTunnelRequestInvocationTimes())
                .isEqualTo(1);
        assertThat(cancelDuringRequestProxyCallback.getOnTunnelHeadersReceivedInvocationTimes())
                .isEqualTo(0);
        assertThat(cancelDuringResponseProxyCallback.getOnTunnelHeadersReceivedInvocationTimes())
                .isEqualTo(1);
        assertThat(cancelDuringResponseProxyCallback.getOnBeforeTunnelRequestInvocationTimes())
                .isEqualTo(1);
    }

    static class AddExtraRequestHeadersProxyCallback extends TestProxyCallback {
        @Override
        public @Nullable List<Map.Entry<String, String>> onBeforeTunnelRequest() {
            super.onBeforeTunnelRequest();
            return Arrays.asList(
                    new AbstractMap.SimpleEntry<>("Authorization", "b3BlbiBzZXNhbWU="),
                    new AbstractMap.SimpleEntry<>("CustomHeader", "CustomValue123"));
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
                                Arrays.asList("HTTP/1.1 502 Bad Gateway"));
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
