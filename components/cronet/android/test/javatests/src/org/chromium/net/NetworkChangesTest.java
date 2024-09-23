// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.SOCK_STREAM;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;
import static com.google.common.truth.TruthJUnit.assume;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.os.ConditionVariable;
import android.system.Os;

import androidx.annotation.OptIn;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.impl.CronetLibraryLoader;

import java.io.FileDescriptor;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.concurrent.CountDownLatch;

/** Test Cronet under different network change scenarios. */
@RunWith(AndroidJUnit4.class)
@DoNotBatch(reason = "crbug/1459563")
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason = "Fake network changes are supported only by the native implementation")
public class NetworkChangesTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private CountDownLatch mHangingUrlLatch;
    private FileDescriptor mSocket;

    private static class Networks {
        private Network mDefaultNetwork;
        private Network mCellular;
        private Network mWifi;

        public Networks(ConnectivityManager connectivityManager) {
            postToInitThreadSync(
                    () -> {
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

            // TODO(crbug.com/40282869): Drop assumes once CQ bots have multiple networks.
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

        public void disconnectDefaultNetwork() {
            fakeNetworkDisconnected(mDefaultNetwork);
        }

        public void connectDefaultNetwork() {
            fakeNetworkConnected(mDefaultNetwork);
            fakeDefaultNetworkChange(mDefaultNetwork);
        }

        private void fakeDefaultNetworkChange(Network network) {
            postToInitThreadSync(
                    () -> {
                        NetworkChangeNotifier.fakeDefaultNetwork(
                                network.getNetworkHandle(), ConnectionType.CONNECTION_4G);
                    });
        }

        private void fakeNetworkDisconnected(Network network) {
            postToInitThreadSync(
                    () -> {
                        NetworkChangeNotifier.fakeNetworkDisconnected(network.getNetworkHandle());
                    });
        }

        private void fakeNetworkConnected(Network network) {
            postToInitThreadSync(
                    () -> {
                        NetworkChangeNotifier.fakeNetworkConnected(
                                network.getNetworkHandle(), ConnectionType.CONNECTION_4G);
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

        QuicTestServer.startQuicTestServer(mTestRule.getTestFramework().getContext());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            JSONObject hostResolverParams =
                                    CronetTestUtil.generateHostResolverRules();
                            JSONObject experimentalOptions =
                                    new JSONObject().put("HostResolverRules", hostResolverParams);
                            builder.setExperimentalOptions(experimentalOptions.toString());

                            builder.enableQuic(true);
                            builder.addQuicHint(
                                    QuicTestServer.getServerHost(),
                                    QuicTestServer.getServerPort(),
                                    QuicTestServer.getServerPort());

                            CronetTestUtil.setMockCertVerifierForTesting(
                                    builder, QuicTestServer.createMockCertVerifier());
                        });

        mHangingUrlLatch = new CountDownLatch(1);
        assertThat(
                        Http2TestServer.startHttp2TestServer(
                                mTestRule.getTestFramework().getContext(), mHangingUrlLatch))
                .isTrue();
    }

    @OptIn(markerClass = org.chromium.net.QuicOptions.Experimental.class)
    private static void disableSessionHandling(CronetEngine.Builder engineBuilder) {
        QuicOptions.Builder optionBuilder = QuicOptions.builder();
        optionBuilder.closeSessionsOnIpChange(false);
        optionBuilder.goawaySessionsOnIpChange(false);
        engineBuilder.setQuicOptions(optionBuilder.build());
    }

    @OptIn(markerClass = org.chromium.net.ConnectionMigrationOptions.Experimental.class)
    private static void disableDefaultNetworkMigration(CronetEngine.Builder engineBuilder) {
        ConnectionMigrationOptions.Builder optionBuilder = ConnectionMigrationOptions.builder();
        optionBuilder.enableDefaultNetworkMigration(false);
        optionBuilder.migrateIdleConnections(false);
        engineBuilder.setConnectionMigrationOptions(optionBuilder.build());
    }

    @OptIn(markerClass = org.chromium.net.QuicOptions.Experimental.class)
    private static void closeSessionsOnIpChange(CronetEngine.Builder engineBuilder) {
        QuicOptions.Builder optionBuilder = QuicOptions.builder();
        optionBuilder.closeSessionsOnIpChange(true);
        optionBuilder.goawaySessionsOnIpChange(false);
        engineBuilder.setQuicOptions(optionBuilder.build());

        disableDefaultNetworkMigration(engineBuilder);
    }

    @OptIn(markerClass = org.chromium.net.QuicOptions.Experimental.class)
    private static void goawayOnIpChange(CronetEngine.Builder engineBuilder) {
        QuicOptions.Builder optionBuilder = QuicOptions.builder();
        optionBuilder.closeSessionsOnIpChange(false);
        optionBuilder.goawaySessionsOnIpChange(true);
        engineBuilder.setQuicOptions(optionBuilder.build());

        disableDefaultNetworkMigration(engineBuilder);
    }

    @OptIn(markerClass = org.chromium.net.ConnectionMigrationOptions.Experimental.class)
    private static void enableDefaultNetworkMigration(CronetEngine.Builder engineBuilder) {
        ConnectionMigrationOptions.Builder optionBuilder = ConnectionMigrationOptions.builder();
        optionBuilder.enableDefaultNetworkMigration(true);
        optionBuilder.migrateIdleConnections(true);
        engineBuilder.setConnectionMigrationOptions(optionBuilder.build());

        disableSessionHandling(engineBuilder);
    }

    @After
    public void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
        mHangingUrlLatch.countDown();
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @SmallTest
    public void testDefaultNetworkChangeBeforeConnect_failsWithErrNetChanged() throws Exception {
        mTestRule.getTestFramework().startEngine();
        // URL pointing at the local socket, where requests will get stuck connecting.
        String url = "https://127.0.0.1:" + ((InetSocketAddress) Os.getsockname(mSocket)).getPort();
        // Launch a few requests at this local port.  Four seems to be the magic number where
        // the last request (and any further request) get stuck connecting.
        TestUrlRequestCallback callback = null;
        UrlRequest request = null;
        for (int i = 0; i < 4; i++) {
            callback = new TestUrlRequestCallback();
            request =
                    mTestRule
                            .getTestFramework()
                            .getEngine()
                            .newUrlRequestBuilder(url, callback, callback.getExecutor())
                            .build();
            request.start();
        }

        waitForStatus(request, UrlRequest.Status.CONNECTING);

        // Simulate network change which should abort connect jobs
        postToInitThreadSync(
                () -> {
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
    @MediumTest
    @DisabledTest(message = "crbug.com/1492515")
    public void testDefaultNetworkChange_spdyCloseSessionsOnIpChange_failsWithErrNetChanged()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            // This ends up throwing away the experimental options set in setUp.
                            // This is fine as those are related to H/3 tests. This is an H/2 so
                            // that's fine. If this assumption stops being true consider merging
                            // them or splitting NetworkChangeTests in two.
                            JSONObject experimentalOptions =
                                    new JSONObject().put("spdy_go_away_on_ip_change", false);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                        });
        mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getHangingRequestUrl();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying SPDY session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        postToInitThreadSync(
                () -> {
                    NetworkChangeNotifier.fakeDefaultNetwork(
                            NetworkChangeNotifier.getInstance().getCurrentDefaultNetId(),
                            ConnectionType.CONNECTION_4G);
                });

        // Similarly to tests below, 15s should be enough for the NCN notification to propagate.
        // In case of a bug (i.e., sessions stop being closed on IP change), don't timeout the test,
        // instead fail here.
        callback.blockForDone(/* timeoutMs= */ 1500);

        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in CronetUrlRequest: net::ERR_NETWORK_CHANGED");
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(NetError.ERR_NETWORK_CHANGED);
    }

    @Test
    @MediumTest
    public void testDefaultNetworkChange_closeSessionsOnIpChange_pendingRequestFails()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            closeSessionsOnIpChange(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // Delay is set to an unreasonably high value as this test doesn't expect this request to
        // succeed
        QuicTestServer.delayResponse("/simple.txt", /* delayInSeconds= */ 100);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.swapDefaultNetwork();

        // Similarly to tests below, 15s should be enough for the NCN notification to propagate.
        // In case of a bug (i.e., sessions stop being closed on IP change), don't timeout after
        // 100s, instead fail here.
        callback.blockForDone(/* timeoutMs= */ 1500);
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError).isInstanceOf(NetworkException.class);
        NetworkException networkException = (NetworkException) callback.mError;
        assertThat(networkException.getErrorCode())
                .isEqualTo(NetworkException.ERROR_NETWORK_CHANGED);
    }

    @Test
    @MediumTest
    public void testDefaultNetworkChange_goAwayonIpChange_pendingRequestSucceeds()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            goawayOnIpChange(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", /* delayInSeconds= */ 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.swapDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
    }

    @Test
    @MediumTest
    public void testDefaultNetworkChange_defaultNetworkMigration_pendingRequestSucceeds()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            enableDefaultNetworkMigration(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", /* delayInSeconds= */ 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.swapDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
    }

    @Test
    @MediumTest
    public void testDoubleDefaultNetworkChange_defaultNetworkMigration_pendingRequestSucceeds()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            enableDefaultNetworkMigration(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", /* delayInSeconds= */ 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.swapDefaultNetwork();
        // Back to previous default network.
        networks.swapDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1486882")
    public void testDefaultNetworkChangeAndDisconnect_goAwayonIpChange_pendingRequestFails()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            goawayOnIpChange(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", /* delayInSeconds= */ 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.swapDefaultNetwork();
        networks.disconnectNonDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError).isInstanceOf(NetworkException.class);
        NetworkException networkException = (NetworkException) callback.mError;
        assertThat(networkException.getErrorCode())
                .isEqualTo(NetworkException.ERROR_NETWORK_CHANGED);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1486878")
    public void
            testDefaultNetworkChangeAndDisconnect_defaultNetworkMigration_pendingRequestSucceeds()
                    throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            enableDefaultNetworkMigration(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", /* delayInSeconds= */ 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.swapDefaultNetwork();
        networks.disconnectNonDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
    }

    @Test
    @MediumTest
    public void
            testDefaultNetworkDisconnectAndReconnect_defaultNetworkMigration_pendingRequestSucceeds()
                    throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            enableDefaultNetworkMigration(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.disconnectDefaultNetwork();
        networks.connectDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
    }

    @Test
    @MediumTest
    public void testDefaultNetworkDisconnectAndReconnect_goawayOnIpChange_pendingRequestSucceeds()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            goawayOnIpChange(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.disconnectDefaultNetwork();
        networks.connectDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
    }

    @Test
    @SmallTest
    public void
            testDefaultNetworkDisconnectAndReconnect_closeSessionsOnIpChange_pendingRequestFails()
                    throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            closeSessionsOnIpChange(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        String url = QuicTestServer.getServerURL() + "/simple.txt";
        // Unfortunately we have no choice but to delay as QuicTestServer doesn't provide any
        // synchronization control to the caller.
        // 15 seconds is, hopefully, a good enough tradeoff between test execution speed and
        // flakiness.
        QuicTestServer.delayResponse("/simple.txt", 15);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        // Without this synchronization it seems that the default network change can happen before
        // the underlying QUIC session is created (read: the test would be flaky).
        waitForStatus(request, UrlRequest.Status.WAITING_FOR_RESPONSE);
        networks.disconnectDefaultNetwork();
        networks.connectDefaultNetwork();

        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in CronetUrlRequest: net::ERR_NETWORK_CHANGED");
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(NetError.ERR_NETWORK_CHANGED);
    }

    @Test
    @SmallTest
    public void testDefaultNetworkChangeAndDisconnect_defaultNetworkMigration_sessionIsMigrated()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            enableDefaultNetworkMigration(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        TestRequestFinishedListener listener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(listener);
        String url = QuicTestServer.getServerURL() + "/simple.txt";

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        callback.blockForDone();
        listener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
        // Socket reused is a poorly named API. What it really represent is whether the underlying
        // session was reused or not (for requests over QUIC, this is populated from
        // https://source.chromium.org/chromium/chromium/src/+/main:net/quic/quic_http_stream.h;l=205;drc=150d8c7e45daeef094be8ec8852e3486eed8f59d).
        assertThat(listener.getRequestInfo().getMetrics().getSocketReused()).isFalse();
        mTestRule.getTestFramework().getEngine().removeRequestFinishedListener(listener);

        // QUIC session created due to the previous request should migrate to the new default
        // network.
        networks.swapDefaultNetwork();

        callback = new TestUrlRequestCallback();
        listener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(listener);
        request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        callback.blockForDone();
        listener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
        // See previous check as to why we're checking for socket being reused.
        assertThat(listener.getRequestInfo().getMetrics().getSocketReused()).isTrue();
        mTestRule.getTestFramework().getEngine().removeRequestFinishedListener(listener);

        // Disconnecting the non-default network should not affect an existing QUIC session.
        networks.disconnectNonDefaultNetwork();

        callback = new TestUrlRequestCallback();
        listener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(listener);
        request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        callback.blockForDone();
        listener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
        // See previous check as to why we're checking for socket being reused.
        assertThat(listener.getRequestInfo().getMetrics().getSocketReused()).isTrue();
        mTestRule.getTestFramework().getEngine().removeRequestFinishedListener(listener);
    }

    @Test
    @SmallTest
    public void testDefaultNetworkChange_goawayOnIpChange_sessionIsNotReused() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            goawayOnIpChange(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        TestRequestFinishedListener listener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(listener);
        String url = QuicTestServer.getServerURL() + "/simple.txt";

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        callback.blockForDone();
        listener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
        // Socket reused is a poorly named API. What it really represent is whether the underlying
        // session was reused or not (for requests over QUIC, this is populated from
        // https://source.chromium.org/chromium/chromium/src/+/main:net/quic/quic_http_stream.h;l=205;drc=150d8c7e45daeef094be8ec8852e3486eed8f59d).
        assertThat(listener.getRequestInfo().getMetrics().getSocketReused()).isFalse();
        mTestRule.getTestFramework().getEngine().removeRequestFinishedListener(listener);

        // This should cause the QUIC session created due to the previous request to be marked as
        // going away.
        networks.swapDefaultNetwork();

        callback = new TestUrlRequestCallback();
        listener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(listener);
        request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        callback.blockForDone();
        listener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
        // See previous check as to why we're checking for socket being reused.
        assertThat(listener.getRequestInfo().getMetrics().getSocketReused()).isFalse();
        mTestRule.getTestFramework().getEngine().removeRequestFinishedListener(listener);
    }

    @Test
    @SmallTest
    public void testDefaultNetworkChange_closeSessionsOnIpChange_sessionIsNotReused()
            throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            closeSessionsOnIpChange(builder);
                        });
        mTestRule.getTestFramework().startEngine();
        ConnectivityManager connectivityManager =
                (ConnectivityManager)
                        mTestRule
                                .getTestFramework()
                                .getContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);
        Networks networks = new Networks(connectivityManager);

        TestRequestFinishedListener listener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(listener);
        String url = QuicTestServer.getServerURL() + "/simple.txt";

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        callback.blockForDone();
        listener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
        // Socket reused is a poorly named API. What it really represent is whether the underlying
        // session was reused or not (for requests over QUIC, this is populated from
        // https://source.chromium.org/chromium/chromium/src/+/main:net/quic/quic_http_stream.h;l=205;drc=150d8c7e45daeef094be8ec8852e3486eed8f59d).
        assertThat(listener.getRequestInfo().getMetrics().getSocketReused()).isFalse();
        mTestRule.getTestFramework().getEngine().removeRequestFinishedListener(listener);

        // This should cause the QUIC session created due to the previous request to be closed.
        networks.swapDefaultNetwork();

        callback = new TestUrlRequestCallback();
        listener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(listener);
        request =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();

        callback.blockForDone();
        listener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("h3");
        // See previous check as to why we're checking for socket being reused.
        assertThat(listener.getRequestInfo().getMetrics().getSocketReused()).isFalse();
        mTestRule.getTestFramework().getEngine().removeRequestFinishedListener(listener);
    }

    private static void waitForStatus(UrlRequest request, int targetStatus) {
        final ConditionVariable cv = new ConditionVariable(/* open= */ false);
        UrlRequest.StatusListener listener =
                new UrlRequest.StatusListener() {
                    @Override
                    public void onStatus(int status) {
                        // We are not guaranteed to receive every single state update: we might
                        // register the listener too late, missing what we're looking for.
                        // Hence, we should unblock also if we see a status that can only happen
                        // after the one we're waiting for (this works under the assumption that
                        // "value ordering" of states represent the "happens-after ordering" of
                        // states, which is true at the moment).
                        if (status >= targetStatus) {
                            cv.open();
                        } else {
                            // Very confusingly, UrlRequest.StatusListener#onStatus gets called
                            // once.
                            // We want to keep getting called until we reach the target status, so
                            // re-register the listener. This effectively means that we're
                            // busy-polling the state, but this is test code, so it's not too bad.
                            // Sleep a bit to avoid potential locks contention.
                            try {
                                Thread.sleep(/* millis= */ 100);
                            } catch (InterruptedException e) {
                                Thread.currentThread().interrupt();
                            }
                            request.getStatus(this);
                        }
                    }
                };
        request.getStatus(listener);
        assertWithMessage("Target status wasn't reached within the given timeout")
                .that(cv.block(/* timeoutMs= */ 5000))
                .isTrue();
    }

    private static void postToInitThreadSync(Runnable r) {
        final ConditionVariable cv = new ConditionVariable(/* open= */ false);
        CronetLibraryLoader.postToInitThread(
                () -> {
                    r.run();
                    cv.open();
                });
        cv.block();
    }
}
