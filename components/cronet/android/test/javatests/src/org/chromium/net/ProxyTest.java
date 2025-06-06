// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/** Test Cronet proxy support. */
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public class ProxyTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    @Before
    public void setUp() throws Exception {
        NativeTestServer.startNativeTestServer(mTestRule.getTestFramework().getContext());
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    static class TestProxyCallback extends Proxy.Callback {
        public AtomicBoolean mCallbackWasInvoked = new AtomicBoolean(false);

        @Override
        public @Nullable List<Map.Entry<String, String>> onBeforeTunnelRequest() {
            mCallbackWasInvoked.set(true);
            return null;
        }

        @Override
        public boolean onTunnelHeadersReceived(
                @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode) {
            mCallbackWasInvoked.set(true);
            return false;
        }
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
        TestProxyCallback proxyCallback = new TestProxyCallback();
        assertThrows(
                NullPointerException.class,
                () ->
                        new Proxy(
                                /* scheme= */ Proxy.HTTPS,
                                /* host= */ null,
                                /* port= */ 8080,
                                /* callback= */ proxyCallback));
    }

    @Test
    @SmallTest
    public void testSetProxyOptions_nullProxyOptions_throws() {
        CronetEngine.Builder builder =
                new CronetEngine.Builder(mTestRule.getTestFramework().getContext());
        assertThrows(NullPointerException.class, () -> builder.setProxyOptions(null));
    }

    @Test
    @SmallTest
    // TODO(https://crbug.com/422429341): Drop this once setProxyOptions impl lands.
    public void testSetProxyOptions_emptyList_throws() {
        CronetEngine.Builder builder =
                new CronetEngine.Builder(mTestRule.getTestFramework().getContext());
        assertThrows(
                UnsupportedOperationException.class,
                () -> builder.setProxyOptions(new ProxyOptions(Collections.emptyList())));
    }

    @Test
    @SmallTest
    // TODO(https://crbug.com/422429341): Drop this once setProxyOptions impl lands.
    public void testSetProxyOptions_nonEmptyList_throws() {
        TestProxyCallback proxyCallback = new TestProxyCallback();
        CronetEngine.Builder builder =
                new CronetEngine.Builder(mTestRule.getTestFramework().getContext());
        assertThrows(
                UnsupportedOperationException.class,
                () ->
                        builder.setProxyOptions(
                                new ProxyOptions(
                                        Arrays.asList(
                                                new Proxy(
                                                        /* scheme= */ Proxy.HTTPS,
                                                        /* host= */ "this-hostname-does-not-exist.com",
                                                        /* port= */ 8080,
                                                        /* callback= */ proxyCallback)))));
    }

    @Test
    @SmallTest
    // TODO(https://crbug.com/422429341): Drop this once setProxyOptions impl lands.
    public void testSetProxyOptions_listWithDirectProxy_throws() {
        CronetEngine.Builder builder =
                new CronetEngine.Builder(mTestRule.getTestFramework().getContext());
        assertThrows(
                UnsupportedOperationException.class,
                () -> builder.setProxyOptions(new ProxyOptions(Arrays.asList((Proxy) null))));
    }

    @Test
    @SmallTest
    @DisabledTest(
            message =
                    "https://crbug.com/422429341: Now that setProxyOptions throws UOE, this"
                            + " requires setProxyOptions being implemented")
    public void testSetProxyOptions_direct_requestSucceeds() {
        TestProxyCallback proxyCallback = new TestProxyCallback();
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
        assertThat(proxyCallback.mCallbackWasInvoked.get()).isFalse();
    }

    @Test
    @SmallTest
    @DisabledTest(
            message =
                    "https://crbug.com/422429341: Now that setProxyOptions throws UOE, this"
                            + " requires setProxyOptions being implemented")
    public void testSetProxyOptions_notExistingProxyWithDirectFallback_requestSucceeds() {
        TestProxyCallback proxyCallback = new TestProxyCallback();
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
        assertThat(proxyCallback.mCallbackWasInvoked.get()).isFalse();
    }

    @Test
    @SmallTest
    @DisabledTest(
            message =
                    "https://crbug.com/422429341: Now that setProxyOptions throws UOE, this"
                            + " requires setProxyOptions being implemented")
    public void testSetProxyOptionsIsANoop_notExistingProxy_requestSucceeds() {
        TestProxyCallback proxyCallback = new TestProxyCallback();
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
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(proxyCallback.mCallbackWasInvoked.get()).isFalse();
    }
}
