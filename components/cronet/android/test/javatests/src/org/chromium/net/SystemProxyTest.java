// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class SystemProxyTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    // This hostname doesn't need to actually work, because Cronet won't try to connect to it - it
    // expects the proxy to connect to it instead.
    //
    // Note that we can't use "localhost" or anything similar here, because //net implicitly
    // bypasses the proxy for local names and IP addresses; see
    // //net/docs/proxy.md and ProxyBypassRules::MatchesImplicitRules().
    private static final String TEST_HOSTNAME = "test-hostname";

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

        try (var nativeTestServerScope =
                new NativeTestServer.PreparedScope(mTestRule.getTestFramework().getContext())) {
            NativeTestServer.registerRequestHandler(requestHandler);
            NativeTestServer.startPrepared();

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
                                    "http.proxyPort", String.valueOf(NativeTestServer.getPort()));
                    var httpsProxyHostScopedSystemProperty =
                            new ScopedSystemProperty("https.proxyHost", "invalid-host");
                    var httpsProxyPortScopedSystemProperty =
                            new ScopedSystemProperty("https.proxyPort", "42")) {
                mTestRule.getTestFramework().startEngine();

                var callback = new TestUrlRequestCallback();
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                "http://" + TEST_HOSTNAME + "/test-path",
                                callback,
                                callback.getExecutor())
                        .build()
                        .start();
                callback.blockForDone();
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

        try (var nativeTestServerScope =
                new NativeTestServer.PreparedScope(mTestRule.getTestFramework().getContext())) {
            NativeTestServer.registerRequestHandler(requestHandler);
            NativeTestServer.startPrepared();

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
                                    "https.proxyPort", String.valueOf(NativeTestServer.getPort()));
                    var httpProxyHostScopedSystemProperty =
                            new ScopedSystemProperty("http.proxyHost", "invalid-host");
                    var httpProxyPortScopedSystemProperty =
                            new ScopedSystemProperty("http.proxyPort", "42")) {
                mTestRule.getTestFramework().startEngine();

                var callback = new TestUrlRequestCallback();
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                "https://" + TEST_HOSTNAME + "/test-path",
                                callback,
                                callback.getExecutor())
                        .build()
                        .start();
                callback.blockForDone();
            }
        }

        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        assertThat(requestHandler.mReceivedHttpRequest.getMethod()).isEqualTo("CONNECT");
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl())
                .isEqualTo(TEST_HOSTNAME + ":443");
    }
}
