// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.SOCK_STREAM;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import android.net.ConnectivityManager;
import android.net.Network;
import android.os.ConditionVariable;
import android.system.Os;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.impl.CronetLibraryLoader;

import java.io.FileDescriptor;
import java.net.InetAddress;
import java.net.InetSocketAddress;

/** Test Cronet under different network change scenarios. */
@RunWith(AndroidJUnit4.class)
@DoNotBatch(reason = "crbug/1459563")
@IgnoreFor(implementations = {CronetImplementation.FALLBACK},
        reason = "Tests implementation details of the native implementation")
public class NetworkChangesTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    private FileDescriptor mSocket;

    private static class Networks {
        private Network mDefaultNetwork;
        private Network mCellular;
        private Network mWifi;

        public Networks(ConnectivityManager connectivityManager) {
            postToInitThreadSync(() -> {
                NetworkChangeNotifierAutoDetect autoDetector =
                        NetworkChangeNotifier.getAutoDetectorForTest();
                assertThat(autoDetector).isNotNull();

                mDefaultNetwork = autoDetector.getDefaultNetwork();

                for (Network network : autoDetector.getNetworksForTesting()) {
                    switch (connectivityManager.getNetworkInfo(network).getType()) {
                        case ConnectivityManager.TYPE_MOBILE:
                            mCellular = network;
                            break;
                        case ConnectivityManager.TYPE_WIFI:
                            mWifi = network;
                            break;
                        default:
                            // Ignore
                    }
                }
            });

            // TODO(crbug.com/1486376): Drop assumes once CQ bots have multiple networks.
            assume().that(mCellular).isNotNull();
            assume().that(mWifi).isNotNull();
            assume().that(mDefaultNetwork).isNotNull();
            assume().that(mDefaultNetwork).isAnyOf(mWifi, mCellular);
            // Protect us against unexpected Network#equals implementation.
            assertThat(mCellular).isNotEqualTo(mWifi);
        }

        public void swapDefaultNetwork() {
            if (isWifiDefault()) {
                makeCellularDefault();
            } else {
                makeWifiDefault();
            }
        }

        public void disconnectNonDefaultNetwork() {
            fakeNetworkDisconnected(getNonDefaultNetwork());
        }

        private void fakeDefaultNetworkChange(Network network) {
            postToInitThreadSync(() -> {
                NetworkChangeNotifier.fakeDefaultNetwork(
                        network.getNetworkHandle(), ConnectionType.CONNECTION_4G);
            });
        }

        private void fakeNetworkDisconnected(Network network) {
            postToInitThreadSync(() -> {
                NetworkChangeNotifier.fakeNetworkDisconnected(network.getNetworkHandle());
            });
        }

        private boolean isWifiDefault() {
            return mDefaultNetwork.equals(mWifi);
        }

        private Network getNonDefaultNetwork() {
            return isWifiDefault() ? mCellular : mWifi;
        }

        private void makeWifiDefault() {
            fakeDefaultNetworkChange(mWifi);
            mDefaultNetwork = mWifi;
        }

        private void makeCellularDefault() {
            fakeDefaultNetworkChange(mCellular);
            mDefaultNetwork = mCellular;
        }
    }

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
        postToInitThreadSync(() -> {
            NetworkChangeNotifier.fakeDefaultNetwork(
                    NetworkChangeNotifier.getInstance().getCurrentDefaultNetId(),
                    ConnectionType.CONNECTION_4G);
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

    @Test
    @SmallTest
    public void testNetworksEmtpyTest() throws Exception {
        // This is to prevent UnusedNestedClass warning from yelling. This will be dropped by a
        // child CL.
        Networks networks;
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

    private static void postToInitThreadSync(Runnable r) {
        final ConditionVariable cv = new ConditionVariable(/*open=*/false);
        CronetLibraryLoader.postToInitThread(() -> {
            r.run();
            cv.open();
        });
        cv.block();
    }
}
