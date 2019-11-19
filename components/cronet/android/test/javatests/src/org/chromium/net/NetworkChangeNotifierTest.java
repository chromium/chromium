// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.SOCK_STREAM;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.assertContains;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;
import android.system.Os;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.impl.CronetLibraryLoader;

import java.io.FileDescriptor;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test NetworkChangeNotifier.
 */
@RunWith(AndroidJUnit4.class)
public class NetworkChangeNotifierTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    /**
     * Verify NetworkChangeNotifier signals trigger appropriate action, like
     * aborting pending connect() jobs.
     */
    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testNetworkChangeNotifier() throws Exception {
        CronetTestFramework testFramework = mTestRule.startCronetTestFramework();
        assertNotNull(testFramework);

        // Os and OsConstants aren't exposed until Lollipop so we cannot run this test.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return;
        }

        // Bind a listening socket to a local port. The socket won't be used to accept any
        // connections, but rather to get connection stuck waiting to connect.
        FileDescriptor s = Os.socket(AF_INET6, SOCK_STREAM, 0);
        // Bind to 127.0.0.1 and a random port (indicated by special 0 value).
        Os.bind(s, InetAddress.getByAddress(null, new byte[] {127, 0, 0, 1}), 0);
        // Set backlog to 0 so connections end up stuck waiting to connect().
        Os.listen(s, 0);

        // Make URL pointing at this local port, where requests will get stuck connecting.
        String url = "http://127.0.0.1:" + ((InetSocketAddress) Os.getsockname(s)).getPort();

        // Launch a few requests at this local port.  Four seems to be the magic number where
        // that request and any further request get stuck connecting.
        TestUrlRequestCallback callback = null;
        UrlRequest request = null;
        for (int i = 0; i < 4; i++) {
            callback = new TestUrlRequestCallback();
            request = testFramework.mCronetEngine
                              .newUrlRequestBuilder(url, callback, callback.getExecutor())
                              .build();
            request.start();
        }

        // Wait for request to get to connecting stage
        final AtomicBoolean requestConnecting = new AtomicBoolean();
        while (!requestConnecting.get()) {
            request.getStatus(new UrlRequest.StatusListener() {
                @Override
                public void onStatus(int status) {
                    requestConnecting.set(status == UrlRequest.Status.CONNECTING);
                }
            });
            Thread.sleep(100);
        }

        // Simulate network change which should abort connect jobs
        CronetLibraryLoader.postToInitThread(new Runnable() {
            @Override
            public void run() {
                NetworkChangeNotifier.getInstance().notifyObserversOfConnectionTypeChange(
                        ConnectionType.CONNECTION_4G);
            }
        });

        // Wait for ERR_NETWORK_CHANGED
        callback.blockForDone();
        assertNotNull(callback.mError);
        assertTrue(callback.mOnErrorCalled);
        assertEquals(NetError.ERR_NETWORK_CHANGED,
                ((NetworkException) callback.mError).getCronetInternalErrorCode());
        assertContains("Exception in CronetUrlRequest: net::ERR_NETWORK_CHANGED",
                callback.mError.getMessage());
    }
}
