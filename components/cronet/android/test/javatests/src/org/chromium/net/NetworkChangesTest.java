// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.SOCK_STREAM;

import static com.google.common.truth.Truth.assertThat;

import android.os.ConditionVariable;
import android.system.Os;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.impl.CronetLibraryLoader;

import java.io.FileDescriptor;
import java.net.InetAddress;
import java.net.InetSocketAddress;

/**
 * Test Cronet under different network change scenarios.
 */
@RunWith(AndroidJUnit4.class)
@DoNotBatch(reason = "crbug/1459563")
@OnlyRunNativeCronet
public class NetworkChangesTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    private FileDescriptor mSocket;

    @Before
    public void setUp() throws Exception {
        // Bind a listening socket to a local port. The socket won't be used to accept any
        // connections, but rather to get connection stuck waiting to connect.
        mSocket = Os.socket(AF_INET6, SOCK_STREAM, 0);
        // Bind to 127.0.0.1 and a random port (indicated by special 0 value).
        Os.bind(mSocket, InetAddress.getByAddress(null, new byte[] {127, 0, 0, 1}), 0);
        // Set backlog to 0 so connections end up stuck waiting to connect().
        Os.listen(mSocket, 0);
    }

    @Test
    @SmallTest
    public void testDefaultNetworkChangeBeforeConnect_failsWithErrNetChanged() throws Exception {
        // URL pointing at the local socket, where requests will get stuck connecting.
        String url = "https://127.0.0.1:" + ((InetSocketAddress) Os.getsockname(mSocket)).getPort();
        // Launch a few requests at this local port.  Four seems to be the magic number where
        // the last request (and any further request) get stuck connecting.
        TestUrlRequestCallback callback = null;
        UrlRequest request = null;
        for (int i = 0; i < 4; i++) {
            callback = new TestUrlRequestCallback();
            request = mTestRule.getTestFramework()
                              .getEngine()
                              .newUrlRequestBuilder(url, callback, callback.getExecutor())
                              .build();
            request.start();
        }

        waitForConnectingStatus(request);

        // Simulate network change which should abort connect jobs
        CronetLibraryLoader.postToInitThread(new Runnable() {
            @Override
            public void run() {
                NetworkChangeNotifier.fakeDefaultNetwork(
                        NetworkChangeNotifier.getInstance().getCurrentDefaultNetId(),
                        ConnectionType.CONNECTION_4G);
            }
        });

        // Wait for ERR_NETWORK_CHANGED
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in CronetUrlRequest: net::ERR_NETWORK_CHANGED");
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(NetError.ERR_NETWORK_CHANGED);
    }

    private static void waitForConnectingStatus(UrlRequest request) {
        final ConditionVariable cv = new ConditionVariable(false /* closed */);
        request.getStatus(new UrlRequest.StatusListener() {
            @Override
            public void onStatus(int status) {
                if (status == UrlRequest.Status.CONNECTING) {
                    cv.open();
                }
            }
        });
        cv.block();
    }
}
