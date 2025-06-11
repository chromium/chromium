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
import org.chromium.base.test.util.DisabledTest;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class SystemProxyTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private static final String TEST_PATH = "/test-path";
    // This URL doesn't need to actually work, because Cronet won't try to connect to it - it
    // expects the proxy to connect to it instead.
    //
    // Note that we can't use "localhost" or anything similar here, because //net implicitly
    // bypasses the proxy for local names and IP addresses; see
    // //net/docs/proxy.md and ProxyBypassRules::MatchesImplicitRules().
    private static final String TEST_URL = "http://test-hostname" + TEST_PATH;

    /**
     * Tests that, if we setup a cleartext HTTP proxy using the standard Java proxy system
     * properties, then Cronet uses that proxy. Also, we send that request to a cleartext HTTP
     * destination, so we expect Cronet to send the request to the proxy using the "traditional"
     * HTTP proxy protocol (a.k.a "message forwarding", a.k.a "absolute-form", not tunnel/CONNECT -
     * see section 3.2.2 of RFC 9112).
     */
    @Test
    @SmallTest
    // TODO(https://crbug.com/424091659): re-enable
    @DisabledTest(
            message =
                    "Fails in Android Platform and seems non-deterministic/order-dependent, see"
                        + " https://crbug.com/424091659")
    public void testSystemProxy_cleartextHttpProxy_sendsPathToProxy() {
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
            try (var proxyHostScopedSystemProperty =
                            new ScopedSystemProperty("http.proxyHost", "localhost");
                    var proxyPortScopedSystemProperty =
                            new ScopedSystemProperty(
                                    "http.proxyPort", String.valueOf(NativeTestServer.getPort()))) {
                mTestRule.getTestFramework().startEngine();

                var callback = new TestUrlRequestCallback();
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(TEST_URL, callback, callback.getExecutor())
                        .build()
                        .start();
                callback.blockForDone();
            }
        }

        assertThat(requestHandler.mReceivedHttpRequest).isNotNull();
        // NativeTestServer actually sees "GET http://.../... HTTP/1.1", but sadly it strips the URL
        // and only passes the path section to the callback. See
        // net::test_server::HttpRequestParser::ParseHeaders().
        assertThat(requestHandler.mReceivedHttpRequest.getRelativeUrl()).isEqualTo(TEST_PATH);
    }
}
