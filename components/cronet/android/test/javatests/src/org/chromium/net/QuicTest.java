// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.chromium.net.CronetTestRule.getTestStorage;
import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.impl.TestLogger;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Date;
import java.util.concurrent.Executors;

/** Tests making requests using QUIC. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason =
                "The fallback implementation doesn't support QUIC. "
                        + "crbug.com/1494870: Enable for AOSP_PLATFORM once fixed")
public class QuicTest {
    private final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();
    private final CronetLoggerTestRule<TestLogger> mLoggerTestRule =
            new CronetLoggerTestRule<>(TestLogger.class);

    @Rule public final RuleChain chain = RuleChain.outerRule(mTestRule).around(mLoggerTestRule);

    private TestLogger mTestLogger;

    @Before
    public void setUp() throws Exception {
        // Load library first, since we need the Quic test server's URL.
        System.loadLibrary("cronet_tests");
        QuicTestServer.startQuicTestServer(mTestRule.getTestFramework().getContext());
        mTestLogger = mLoggerTestRule.mTestLogger;
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableNetworkQualityEstimator(true).enableQuic(true);
                            builder.addQuicHint(
                                    QuicTestServer.getServerHost(),
                                    QuicTestServer.getServerPort(),
                                    QuicTestServer.getServerPort());

                            // The pref may not be written if the computed Effective Connection Type
                            // (ECT) matches the default ECT for the current connection type.
                            // Force the ECT to "Slow-2G". Since "Slow-2G" is not the default ECT
                            // for any connection type, this ensures that the pref is written to.
                            JSONObject nqeParams =
                                    new JSONObject()
                                            .put("force_effective_connection_type", "Slow-2G");

                            // TODO(mgersh): Enable connection migration once it works, see
                            // http://crbug.com/634910
                            JSONObject quicParams =
                                    new JSONObject()
                                            .put("connection_options", "PACE,IW10,FOO,DEADBEEF")
                                            .put("max_server_configs_stored_in_properties", 2)
                                            .put("idle_connection_timeout_seconds", 300)
                                            // Disable Retry on TCP when QUIC fails before headers
                                            // are received
                                            .put("retry_without_alt_svc_on_quic_errors", false)
                                            .put("migrate_sessions_on_network_change_v2", false)
                                            .put("migrate_sessions_early_v2", false)
                                            .put("race_cert_verification", true);
                            JSONObject hostResolverParams =
                                    CronetTestUtil.generateHostResolverRules();
                            JSONObject experimentalOptions =
                                    new JSONObject()
                                            .put("QUIC", quicParams)
                                            .put("HostResolverRules", hostResolverParams)
                                            .put("NetworkQualityEstimator", nqeParams);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                            builder.setStoragePath(
                                    getTestStorage(mTestRule.getTestFramework().getContext()));
                            builder.enableHttpCache(
                                    CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, 1000 * 1024);
                            CronetTestUtil.setMockCertVerifierForTesting(
                                    builder, QuicTestServer.createMockCertVerifier());
                        });
        mTestRule.getTestFramework().startEngine();
    }

    @After
    public void tearDown() throws Exception {
        mTestLogger = null;
        QuicTestServer.shutdownQuicTestServer();
    }

    @Test
    @LargeTest
    public void testQuicLoadUrl() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().getEngine();
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertThat(callback.mResponseAsString).isEqualTo(expectedContent);
        assertIsQuic(callback.getResponseInfoWithChecks());
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertThat(callback.getResponseInfoWithChecks())
                .hasReceivedByteCountThat()
                .isGreaterThan((long) expectedContent.length());
        ((CronetUrlRequestContext) cronetEngine).flushWritePropertiesForTesting();
        assertThat(
                        fileContainsString(
                                "local_prefs.json",
                                QuicTestServer.getServerHost()
                                        + ":"
                                        + QuicTestServer.getServerPort()))
                .isTrue();
        cronetEngine.shutdown();

        // Make another request using a new context but with no QUIC hints.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(mTestRule.getTestFramework().getContext());
        builder.setStoragePath(getTestStorage(mTestRule.getTestFramework().getContext()));
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        builder.enableQuic(true);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions =
                new JSONObject().put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        cronetEngine = builder.build();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback2, callback2.getExecutor());
        requestBuilder.build().start();
        callback2.blockForDone();
        assertThat(callback2.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback2.mResponseAsString).isEqualTo(expectedContent);
        assertIsQuic(callback.getResponseInfoWithChecks());
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertThat(callback2.getResponseInfoWithChecks())
                .hasReceivedByteCountThat()
                .isGreaterThan((long) expectedContent.length());
        cronetEngine.shutdown();
    }

    // Returns whether a file contains a particular string.
    private boolean fileContainsString(String filename, String content) throws IOException {
        File file =
                new File(
                        getTestStorage(mTestRule.getTestFramework().getContext())
                                + "/prefs/"
                                + filename);
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }

    /** Tests that the network quality listeners are propoerly notified when QUIC is enabled. */
    @Test
    @LargeTest
    @SuppressWarnings("deprecation")
    public void testNQEWithQuic() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().getEngine();
        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";

        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(Executors.newSingleThreadExecutor());
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(Executors.newSingleThreadExecutor());

        cronetEngine.addRttListener(rttListener);
        cronetEngine.addThroughputListener(throughputListener);

        cronetEngine.configureNetworkQualityEstimatorForTesting(true, true, true);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        // Although the native stack races QUIC and SPDY for the first request,
        // since there is no http server running on the corresponding TCP port,
        // QUIC will always succeed with a 200 (see
        // net::HttpStreamFactoryImpl::Request::OnStreamFailed).
        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertThat(callback.mResponseAsString).isEqualTo(expectedContent);
        assertIsQuic(callback.getResponseInfoWithChecks());

        // Throughput observation is posted to the network quality estimator on the network thread
        // after the UrlRequest is completed. The observations are then eventually posted to
        // throughput listeners on the executor provided to network quality.
        throughputListener.waitUntilFirstThroughputObservationReceived();

        // Wait for RTT observation (at the URL request layer) to be posted.
        rttListener.waitUntilFirstUrlRequestRTTReceived();

        assertThat(throughputListener.throughputObservationCount()).isGreaterThan(0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertThat(rttListener.rttObservationCount(0)).isGreaterThan(0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertThat(rttListener.rttObservationCount(2)).isGreaterThan(0);

        // Verify that effective connection type callback is received and
        // effective connection type is correctly set.
        assertThat(cronetEngine.getEffectiveConnectionType())
                .isNotEqualTo(EffectiveConnectionType.TYPE_UNKNOWN);

        // Verify that the HTTP RTT, transport RTT and downstream throughput
        // estimates are available.
        assertThat(cronetEngine.getHttpRttMs()).isAtLeast(0);
        assertThat(cronetEngine.getTransportRttMs()).isAtLeast(0);
        assertThat(cronetEngine.getDownstreamThroughputKbps()).isAtLeast(0);

        ((CronetUrlRequestContext) cronetEngine).flushWritePropertiesForTesting();
        assertThat(fileContainsString("local_prefs.json", "network_qualities")).isTrue();
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testMetricsWithQuic() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().getEngine();
        CronetImplementation implementationUnderTest = mTestRule.implementationUnderTest();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        cronetEngine.addRequestFinishedListener(requestFinishedListener);

        String quicURL = QuicTestServer.getServerURL() + "/simple.txt";
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback, callback.getExecutor());
        Date startTime = new Date();
        requestBuilder.build().start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertIsQuic(callback.getResponseInfoWithChecks());

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                implementationUnderTest, requestInfo, quicURL, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                implementationUnderTest, requestInfo.getMetrics(), startTime, endTime, true);

        // Second request should use the same connection and not have ConnectTiming numbers
        callback = new TestUrlRequestCallback();
        requestFinishedListener.reset();
        requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback, callback.getExecutor());
        startTime = new Date();
        requestBuilder.build().start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        endTime = new Date();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertIsQuic(callback.getResponseInfoWithChecks());

        requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                implementationUnderTest, requestInfo, quicURL, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkNoConnectTiming(implementationUnderTest, requestInfo.getMetrics());

        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testQuicCloseConnectionFromServer() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().getEngine();
        String quicURL = QuicTestServer.getServerURL() + QuicTestServer.getConnectionClosePath();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        UrlRequest.Builder requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfo()).isNull();
        assertThat(callback.mError).isInstanceOf(QuicException.class);
        QuicException quicException = (QuicException) callback.mError;
        // 0 is QUIC_NO_ERROR, This is expected because of the test-server behavior, see
        // https://source.chromium.org/chromium/_/quiche/quiche/+/86e3e869377b05a7143dfa07a4d1219881396661:quiche/quic/tools/quic_simple_server_stream.cc;l=286;
        assertThat(quicException.getQuicDetailedErrorCode()).isEqualTo(0);
        assertThat(quicException.getConnectionCloseSource()).isEqualTo(ConnectionCloseSource.PEER);
        assertThat(quicException.getErrorCode())
                .isEqualTo(NetworkException.ERROR_QUIC_PROTOCOL_FAILED);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mTestLogger.waitForLogCronetTrafficInfo();
            final CronetTrafficInfo trafficInfo = mTestLogger.getLastCronetTrafficInfo();
            assertThat(trafficInfo.getConnectionCloseSource())
                    .isEqualTo(quicException.getConnectionCloseSource());
            assertThat(trafficInfo.getNetworkInternalErrorCode())
                    .isEqualTo(quicException.getCronetInternalErrorCode());
            assertThat(trafficInfo.getQuicErrorCode())
                    .isEqualTo(quicException.getQuicDetailedErrorCode());
            assertThat(trafficInfo.getFailureReason())
                    .isEqualTo(CronetTrafficInfo.RequestFailureReason.NETWORK);
        }
    }

    // Helper method to assert that the request is negotiated over QUIC.
    private void assertIsQuic(UrlResponseInfo responseInfo) {
        String protocol = responseInfo.getNegotiatedProtocol();
        assertWithMessage("Expected the negotiatedProtocol to be QUIC but was " + protocol)
                .that(protocol.startsWith("http/2+quic") || protocol.startsWith("h3"))
                .isTrue();
    }
}
