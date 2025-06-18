// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyList;
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
import org.mockito.Mockito;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;

import java.util.AbstractMap;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

/** Test Cronet proxy support. */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class ProxyTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private NativeTestServer.PreparedScope mNativeTestServerScope;

    @Before
    public void setUp() throws Exception {
        mNativeTestServerScope =
                new NativeTestServer.PreparedScope(mTestRule.getTestFramework().getContext());
    }

    @After
    public void tearDown() throws Exception {
        mNativeTestServerScope.close();
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
        NativeTestServer.startPrepared();
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
                        NativeTestServer.getSuccessURL(), callback, callback.getExecutor());
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
        NativeTestServer.startPrepared();
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
                        NativeTestServer.getSuccessURL(), callback, callback.getExecutor());
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
        NativeTestServer.startPrepared();
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
                        NativeTestServer.getSuccessURL(), callback, callback.getExecutor());
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
        NativeTestServer.startPrepared();
        Proxy.Callback unreachableProxyCallback = Mockito.mock(Proxy.Callback.class);
        Mockito.when(unreachableProxyCallback.onBeforeTunnelRequest()).thenReturn(null);
        Proxy.Callback reachableProxyCallback = Mockito.mock(Proxy.Callback.class);
        Mockito.when(reachableProxyCallback.onBeforeTunnelRequest())
                .thenReturn(Collections.emptyList());
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
                                                                /* port= */ NativeTestServer
                                                                        .getPort(),
                                                                /* callback= */ unreachableProxyCallback),
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ NativeTestServer
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        NativeTestServer.registerRequestHandler(requestHandler);
        NativeTestServer.startPrepared();
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
                                                                /* port= */ NativeTestServer
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        NativeTestServer.registerRequestHandler(requestHandler);
        NativeTestServer.startPrepared();
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
        Mockito.when(proxyCallback.onBeforeTunnelRequest()).thenReturn(Collections.emptyList());
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
                                                                /* port= */ NativeTestServer
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
    public void testExtraRequestHeadersAreSent() {
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
        NativeTestServer.registerRequestHandler(requestHandler);
        NativeTestServer.startPrepared();
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
        Mockito.when(proxyCallback.onBeforeTunnelRequest())
                .thenReturn(
                        Arrays.asList(
                                new AbstractMap.SimpleEntry<>(
                                        "Authorization", "b3BlbiBzZXNhbWU=")));
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
                                                                /* port= */ NativeTestServer
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
    @DisabledTest(
            message =
                    "TODO(https://crbug.com/424790520): Enable this once we can craft a proper"
                            + " response. This might require extending NativeTestServer to support"
                            + " CONNECT.")
    public void testResponseHeadersAreReceived() {
        var requestHandler =
                new NativeTestServer.HandleRequestCallback() {
                    public NativeTestServer.HttpRequest mReceivedHttpRequest;

                    @Override
                    public NativeTestServer.RawHttpResponse handleRequest(
                            NativeTestServer.HttpRequest httpRequest) {
                        assertThat(mReceivedHttpRequest).isNull();
                        mReceivedHttpRequest = httpRequest;
                        // TODO(https://crbug.com/424790520): Craft a proper response, or extend
                        // NativeTestServer to support CONNECT.")
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        NativeTestServer.registerRequestHandler(requestHandler);
        NativeTestServer.startPrepared();
        Proxy.Callback proxyCallback = Mockito.mock(Proxy.Callback.class);
        Mockito.when(proxyCallback.onBeforeTunnelRequest()).thenReturn(Collections.emptyList());
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
                                                                /* port= */ NativeTestServer
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
        Mockito.verify(proxyCallback, times(1)).onTunnelHeadersReceived(anyList(), eq(400));
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        NativeTestServer.registerRequestHandler(requestHandler);
        NativeTestServer.startPrepared();
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
                                                                /* port= */ NativeTestServer
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        NativeTestServer.registerRequestHandler(requestHandler);
        NativeTestServer.startPrepared();
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
                                                                /* port= */ NativeTestServer
                                                                        .getPort(),
                                                                /* callback= */ cancelDuringRequestProxyCallback),
                                                        new Proxy(
                                                                /* scheme= */ Proxy.HTTP,
                                                                /* host= */ "localhost",
                                                                /* port= */ NativeTestServer
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
                        return new NativeTestServer.RawHttpResponse("", "");
                    }
                };
        NativeTestServer.registerRequestHandler(requestHandler);
        NativeTestServer.startPrepared();
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
                                                                /* port= */ NativeTestServer
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
