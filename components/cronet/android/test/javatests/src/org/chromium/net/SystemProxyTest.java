// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Proxy;
import android.os.Handler;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;

import java.util.Collections;
import java.util.HashMap;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class SystemProxyTest {
    private static final String TAG = "SystemProxyTest";
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    // This hostname doesn't need to actually work, because Cronet won't try to connect to it - it
    // expects the proxy to connect to it instead.
    //
    // Note that we can't use "localhost" or anything similar here, because //net implicitly
    // bypasses the proxy for local names and IP addresses; see
    // //net/docs/proxy.md and ProxyHostMatchingRules::MatchesImplicitRules().
    private static final String TEST_HOSTNAME = "test-hostname";

    private static final class BroadcastContext extends ContextWrapper {
        private final HashMap<BroadcastReceiver, IntentFilter> mReceivers = new HashMap<>();

        BroadcastContext(Context context) {
            super(context);
        }

        @Override
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String broadcastPermission,
                Handler scheduler) {
            var result =
                    getBaseContext()
                            .registerReceiver(receiver, filter, broadcastPermission, scheduler);
            mReceivers.put(receiver, filter);
            return result;
        }

        @Override
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String broadcastPermission,
                Handler scheduler,
                int flags) {
            var result =
                    getBaseContext()
                            .registerReceiver(
                                    receiver, filter, broadcastPermission, scheduler, flags);
            mReceivers.put(receiver, filter);
            return result;
        }

        @Override
        public void unregisterReceiver(BroadcastReceiver receiver) {
            getBaseContext().unregisterReceiver(receiver);
            assert mReceivers.remove(receiver) != null;
        }

        public void injectBroadcast(Intent intent) {
            for (var receiverEntry : mReceivers.entrySet()) {
                var filter = receiverEntry.getValue();
                if (filter != null
                        && filter.match(/* resolver= */ null, intent, /* resolve= */ false, TAG)
                                < 0) {
                    continue;
                }
                receiverEntry.getKey().onReceive(this, intent);
            }
        }
    }

    private BroadcastContext mBroadcastContext;

    @Before
    public void interceptBroadcastReceiver() {
        mTestRule
                .getTestFramework()
                .interceptContext(
                        new ContextInterceptor() {
                            @Override
                            public Context interceptContext(Context context) {
                                mBroadcastContext = new BroadcastContext(context);
                                return mBroadcastContext;
                            }
                        });
    }

    private static class NativeTestServerRequestHandler
            implements NativeTestServer.HandleRequestCallback {
        public NativeTestServer.HttpRequest mReceivedHttpRequest;

        @Override
        public NativeTestServer.RawHttpResponse handleRequest(
                NativeTestServer.HttpRequest httpRequest) {
            assertThat(mReceivedHttpRequest).isNull();
            mReceivedHttpRequest = httpRequest;
            return NativeTestServer.RawHttpResponse.createFromHeaders(Collections.emptyList());
        }
    }

    private void executeRequest(String scheme) {
        var callback = new TestUrlRequestCallback();
        mTestRule
                .getTestFramework()
                .getEngine()
                .newUrlRequestBuilder(
                        scheme + "://" + TEST_HOSTNAME + "/test-path",
                        callback,
                        callback.getExecutor())
                .build()
                .start();
        callback.blockForDone();
    }

    /**
     * Tests that, if we setup a proxy for the http:// scheme using the standard Java proxy system
     * properties, then Cronet uses that proxy for http:// destinations. Also, because we send that
     * request to a cleartext HTTP destination, we expect Cronet to send the request to the proxy
     * using the "traditional" HTTP proxy protocol (a.k.a "message forwarding", a.k.a
     * "absolute-form", not tunnel/CONNECT - see section 3.2.2 of RFC 9112).
     */
    @Test
    @SmallTest
    public void testHttpScheme_sendsPathToProxy() {
        var requestHandler = new NativeTestServerRequestHandler();

        try (var nativeTestServer =
                NativeTestServer.createNativeTestServer(
                        mTestRule.getTestFramework().getContext())) {
            nativeTestServer.registerRequestHandler(requestHandler);
            nativeTestServer.start();

            // These are the standard Java system properties for discovering system proxies,
            // see:
            //
            // https://docs.oracle.com/en/java/javase/24/docs/api/java.base/java/net/doc-files/net-properties.html
            //
            // Android supports these, and will set them if the OS is configured to use a system
            // proxy.
            //
            // Note we also set the https properties to invalid values; this way, we also check that
            // Cronet ignores these properties. Indeed the "http." part in the system property key
            // refers to the scheme of the *destination*, not the scheme used to talk to the proxy.
            try (var httpProxyHostScopedSystemProperty =
                            new ScopedSystemProperty("http.proxyHost", "localhost");
                    var httpProxyPortScopedSystemProperty =
                            new ScopedSystemProperty(
                                    "http.proxyPort", String.valueOf(nativeTestServer.getPort()));
                    var httpsProxyHostScopedSystemProperty =
                            new ScopedSystemProperty("https.proxyHost", "invalid-host");
                    var httpsProxyPortScopedSystemProperty =
                            new ScopedSystemProperty("https.proxyPort", "42")) {
                mTestRule.getTestFramework().startEngine();
                executeRequest("http");
            }
        }

        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("GET");
        // NativeTestServer actually sees "GET http://.../... HTTP/1.1", but sadly it strips the URL
        // and only passes the path section to the callback. See
        // net::test_server::HttpRequestParser::ParseHeaders().
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl()).isEqualTo("/test-path");
    }

    /**
     * Tests that, if we setup a proxy for the https:// scheme using the standard Java proxy system
     * properties, then Cronet uses that proxy for https:// destinations. Also, because we send that
     * request to a secure HTTPS destination, we expect Cronet to send the request through the proxy
     * using the tunnel mechanism (e.g. CONNECT HTTP method).
     *
     * <p>Note: there is no test for using HTTPS to talk to the proxy itself because that is not
     * supported for system proxies - Android system configuration does not have any way to express
     * that.
     */
    @Test
    @SmallTest
    public void testHttpsScheme_usesConnect() {
        var requestHandler = new NativeTestServerRequestHandler();

        try (var nativeTestServer =
                NativeTestServer.createNativeTestServer(
                        mTestRule.getTestFramework().getContext())) {
            nativeTestServer.registerRequestHandler(requestHandler);
            nativeTestServer.start();

            // These are the standard Java system properties for discovering system proxies,
            // see:
            //
            // https://docs.oracle.com/en/java/javase/24/docs/api/java.base/java/net/doc-files/net-properties.html
            //
            // Android supports these, and will set them if the OS is configured to use a system
            // proxy.
            //
            // Note we also set the http properties to invalid values; this way, we also check that
            // Cronet ignores these properties. Indeed the "https." part in the system property key
            // refers to the scheme of the *destination*, not the scheme used to talk to the proxy.
            try (var httpsProxyHostScopedSystemProperty =
                            new ScopedSystemProperty("https.proxyHost", "localhost");
                    var httpsProxyPortScopedSystemProperty =
                            new ScopedSystemProperty(
                                    "https.proxyPort", String.valueOf(nativeTestServer.getPort()));
                    var httpProxyHostScopedSystemProperty =
                            new ScopedSystemProperty("http.proxyHost", "invalid-host");
                    var httpProxyPortScopedSystemProperty =
                            new ScopedSystemProperty("http.proxyPort", "42")) {
                mTestRule.getTestFramework().startEngine();
                executeRequest("https");
            }
        }

        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("CONNECT");
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl())
                .isEqualTo(TEST_HOSTNAME + ":443");
    }

    /** Tests that Cronet reacts to proxy configuration changes while an engine is running. */
    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "Fallback doesn't support proxy change broadcasts")
    public void testProxyChange() throws Exception {
        var requestHandler = new NativeTestServerRequestHandler();

        try (var nativeTestServer =
                NativeTestServer.createNativeTestServer(
                        mTestRule.getTestFramework().getContext())) {
            nativeTestServer.registerRequestHandler(requestHandler);
            nativeTestServer.start();

            // Set up a proxy config and verify that it is used, same as
            // testHttpScheme_sendsPathToProxy().
            try (var httpProxyHostScopedSystemProperty =
                            new ScopedSystemProperty("http.proxyHost", "localhost");
                    var httpProxyPortScopedSystemProperty =
                            new ScopedSystemProperty(
                                    "http.proxyPort", String.valueOf(nativeTestServer.getPort()))) {
                mTestRule.getTestFramework().startEngine();
                executeRequest("http");
            }
            assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
            requestHandler.mReceivedHttpRequest = null;

            // Do it again, with the properties unset. We expect Cronet to keep using the above
            // proxy config as it was not notified the proxy config changed. This is mostly to
            // ensure the test is testing what we think it's testing (i.e. proxy change broadcast
            // listening logic).
            executeRequest("http");
            assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
            requestHandler.mReceivedHttpRequest = null;

            // Now, notify Cronet that the proxy config changed. Note that, when Cronet receives
            // this broadcast, it does not use the above properties to refresh its proxy config -
            // instead, it queries ConnectivityManager (see
            // org.chromium.net.ProxyChangeListener#getProxyConfig). Mocking ConnectivityManager
            // is tricky, so we don't attempt to return a proxy config from ConnectivityManager - we
            // just assume it will return something different from the above.
            mBroadcastContext.injectBroadcast(new Intent(Proxy.PROXY_CHANGE_ACTION));

            // Cronet acts on proxy updates asynchronously in the init thread, so check repeatedly
            // to mitigate races.
            for (int tryCount = 0; tryCount < 100; tryCount++) {
                executeRequest("http");
                if (requestHandler.mReceivedHttpRequest == null) {
                    return;
                }
                requestHandler.mReceivedHttpRequest = null;
                Thread.sleep(50);
            }

            fail("Cronet keeps sending requests through the proxy despite proxy config change");
        }
    }
}
