// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.getContext;

import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.NativeTestServer;

import java.net.HttpURLConnection;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.URL;

/**
 * Tests for CronetHttpURLStreamHandler class.
 */
@RunWith(AndroidJUnit4.class)
public class CronetHttpURLStreamHandlerTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private CronetTestFramework mTestFramework;

    @Before
    public void setUp() throws Exception {
        mTestFramework = mTestRule.startCronetTestFramework();
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionHttp() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestFramework.mCronetEngine);
        HttpURLConnection connection =
                (HttpURLConnection) streamHandler.openConnection(url);
        assertEquals(200, connection.getResponseCode());
        assertEquals("OK", connection.getResponseMessage());
        assertEquals("GET", TestUtil.getResponseAsString(connection));
        connection.disconnect();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionHttps() throws Exception {
        URL url = new URL("https://example.com");
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestFramework.mCronetEngine);
        HttpURLConnection connection =
                (HttpURLConnection) streamHandler.openConnection(url);
        assertNotNull(connection);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionProtocolNotSupported() throws Exception {
        URL url = new URL("ftp://example.com");
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestFramework.mCronetEngine);
        try {
            streamHandler.openConnection(url);
            fail();
        } catch (UnsupportedOperationException e) {
            assertEquals("Unexpected protocol:ftp", e.getMessage());
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    public void testOpenConnectionWithProxy() throws Exception {
        URL url = new URL(NativeTestServer.getEchoMethodURL());
        CronetHttpURLStreamHandler streamHandler =
                new CronetHttpURLStreamHandler(mTestFramework.mCronetEngine);
        Proxy proxy = new Proxy(Proxy.Type.HTTP,
                new InetSocketAddress("127.0.0.1", 8080));
        try {
            streamHandler.openConnection(url, proxy);
            fail();
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
    }
}
