// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.NativeTestServer;

import java.net.HttpURLConnection;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.URL;

/** Tests for CronetHttpURLStreamHandler class. */
@Batch(Batch.UNIT_TESTS)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "See crrev.com/c/4590329")
@RunWith(AndroidJUnit4.class)
public class CronetHttpURLStreamHandlerTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    @Before
    public void setUp() throws Exception {
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    public void testOpenConnectionHttp() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestRule.getTestFramework().getEngine());
        HttpURLConnection connection = (HttpURLConnection) streamHandler.openConnection(url);
        assertThat(connection.getResponseCode()).isEqualTo(200);
        assertThat(connection.getResponseMessage()).isEqualTo("OK");
        assertThat(TestUtil.getResponseAsString(connection)).isEqualTo("GET");
        connection.disconnect();
    }

    @Test
    @SmallTest
    public void testOpenConnectionHttps() throws Exception {
        URL url = new URL("https://example.com");
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestRule.getTestFramework().getEngine());
        HttpURLConnection connection = (HttpURLConnection) streamHandler.openConnection(url);
        assertThat(connection).isNotNull();
    }

    @Test
    @SmallTest
    public void testOpenConnectionProtocolNotSupported() throws Exception {
        URL url = new URL("ftp://example.com");
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestRule.getTestFramework().getEngine());
        UnsupportedOperationException e =
                assertThrows(
                        UnsupportedOperationException.class,
                        () -> streamHandler.openConnection(url));
        assertThat(e).hasMessageThat().isEqualTo("Unexpected protocol:ftp");
    }

    @Test
    @SmallTest
    @SuppressWarnings("AddressSelection")
    public void testOpenConnectionWithProxy() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestRule.getTestFramework().getEngine());
        Proxy proxy = new Proxy(Proxy.Type.HTTP, new InetSocketAddress("127.0.0.1", 8080));
        assertThrows(
                UnsupportedOperationException.class,
                () -> streamHandler.openConnection(url, proxy));
    }
}
