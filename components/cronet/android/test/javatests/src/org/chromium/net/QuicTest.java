// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.getContext;
import static org.chromium.net.CronetTestRule.getTestStorage;

import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.MetricsTestUtil.TestRequestFinishedListener;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Date;
import java.util.concurrent.Executors;

/**
 * Tests making requests using QUIC.
 */
@RunWith(AndroidJUnit4.class)
public class QuicTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private static final String TAG = QuicTest.class.getSimpleName();
    private static final String QUIC_PROTOCOL_STRING_PREFIX = "http/2+quic/";
    private ExperimentalCronetEngine.Builder mBuilder;

    @Before
    public void setUp() throws Exception {
        // Load library first, since we need the Quic test server's URL.
        System.loadLibrary("cronet_tests");
        QuicTestServer.startQuicTestServer(getContext());

        mBuilder = new ExperimentalCronetEngine.Builder(getContext());
        mBuilder.enableNetworkQualityEstimator(true).enableQuic(true);
        mBuilder.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());

        // The pref may not be written if the computed Effective Connection Type (ECT) matches the
        // default ECT for the current connection type. Force the ECT to "Slow-2G". Since "Slow-2G"
        // is not the default ECT for any connection type, this ensures that the pref is written to.
        JSONObject nqeParams = new JSONObject().put("force_effective_connection_type", "Slow-2G");

        // TODO(mgersh): Enable connection migration once it works, see http://crbug.com/634910
        JSONObject quicParams = new JSONObject()
                                        .put("connection_options", "PACE,IW10,FOO,DEADBEEF")
                                        .put("max_server_configs_stored_in_properties", 2)
                                        .put("idle_connection_timeout_seconds", 300)
                                        .put("migrate_sessions_on_network_change_v2", false)
                                        .put("migrate_sessions_early_v2", false)
                                        .put("race_cert_verification", true);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("QUIC", quicParams)
                                                 .put("HostResolverRules", hostResolverParams)
                                                 .put("NetworkQualityEstimator", nqeParams);
        mBuilder.setExperimentalOptions(experimentalOptions.toString());
        mBuilder.setStoragePath(getTestStorage(getContext()));
        mBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, 1000 * 1024);
        CronetTestUtil.setMockCertVerifierForTesting(
                mBuilder, QuicTestServer.createMockCertVerifier());
    }

    @After
    public void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
    }

    @Test
    @LargeTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testQuicLoadUrl() throws Exception {
        ExperimentalCronetEngine cronetEngine = mBuilder.build();
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

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertEquals(expectedContent, callback.mResponseAsString);
        assertIsQuic(callback.mResponseInfo);
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(callback.mResponseInfo.getReceivedByteCount() > expectedContent.length());
        // This test takes a long time, since the update will only be scheduled
        // after kUpdatePrefsDelayMs in http_server_properties_manager.cc.
        while (true) {
            Log.i(TAG, "Still waiting for pref file update.....");
            Thread.sleep(10000);
            boolean contains = false;
            try {
                if (fileContainsString("local_prefs.json", "quic")) break;
            } catch (FileNotFoundException e) {
                // Ignored this exception since the file will only be created when updates are
                // flushed to the disk.
            }
        }
        assertTrue(fileContainsString("local_prefs.json",
                QuicTestServer.getServerHost() + ":" + QuicTestServer.getServerPort()));
        cronetEngine.shutdown();

        // Make another request using a new context but with no QUIC hints.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.setStoragePath(getTestStorage(getContext()));
        builder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        builder.enableQuic(true);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        cronetEngine = builder.build();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        requestBuilder =
                cronetEngine.newUrlRequestBuilder(quicURL, callback2, callback2.getExecutor());
        requestBuilder.build().start();
        callback2.blockForDone();
        assertEquals(200, callback2.mResponseInfo.getHttpStatusCode());
        assertEquals(expectedContent, callback2.mResponseAsString);
        assertIsQuic(callback.mResponseInfo);
        // The total received bytes should be larger than the content length, to account for
        // headers.
        assertTrue(callback2.mResponseInfo.getReceivedByteCount() > expectedContent.length());
        cronetEngine.shutdown();
    }

    // Returns whether a file contains a particular string.
    private boolean fileContainsString(String filename, String content) throws IOException {
        File file = new File(getTestStorage(getContext()) + "/prefs/" + filename);
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }

    /**
     * Tests that the network quality listeners are propoerly notified when QUIC is enabled.
     */
    @Test
    @LargeTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    @SuppressWarnings("deprecation")
    public void testNQEWithQuic() throws Exception {
        ExperimentalCronetEngine cronetEngine = mBuilder.build();
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

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String expectedContent = "This is a simple text file served by QUIC.\n";
        assertEquals(expectedContent, callback.mResponseAsString);
        assertIsQuic(callback.mResponseInfo);

        // Throughput observation is posted to the network quality estimator on the network thread
        // after the UrlRequest is completed. The observations are then eventually posted to
        // throughput listeners on the executor provided to network quality.
        throughputListener.waitUntilFirstThroughputObservationReceived();

        // Wait for RTT observation (at the URL request layer) to be posted.
        rttListener.waitUntilFirstUrlRequestRTTReceived();

        assertTrue(throughputListener.throughputObservationCount() > 0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertTrue(rttListener.rttObservationCount(0) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertTrue(rttListener.rttObservationCount(2) > 0);

        // Verify that effective connection type callback is received and
        // effective connection type is correctly set.
        assertTrue(
                cronetEngine.getEffectiveConnectionType() != EffectiveConnectionType.TYPE_UNKNOWN);

        // Verify that the HTTP RTT, transport RTT and downstream throughput
        // estimates are available.
        assertTrue(cronetEngine.getHttpRttMs() >= 0);
        assertTrue(cronetEngine.getTransportRttMs() >= 0);
        assertTrue(cronetEngine.getDownstreamThroughputKbps() >= 0);

        // Verify that the cached estimates were written to the prefs.
        while (true) {
            Log.i(TAG, "Still waiting for pref file update.....");
            Thread.sleep(10000);
            try {
                if (fileContainsString("local_prefs.json", "network_qualities")) {
                    break;
                }
            } catch (FileNotFoundException e) {
                // Ignored this exception since the file will only be created when updates are
                // flushed to the disk.
            }
        }
        assertTrue(fileContainsString("local_prefs.json", "network_qualities"));
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @Feature({"Cronet"})
    public void testMetricsWithQuic() throws Exception {
        ExperimentalCronetEngine cronetEngine = mBuilder.build();
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

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertIsQuic(callback.mResponseInfo);

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, quicURL, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(requestInfo.getMetrics(), startTime, endTime, true);

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

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertIsQuic(callback.mResponseInfo);

        requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(requestInfo, quicURL, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, requestInfo.getFinishedReason());
        MetricsTestUtil.checkNoConnectTiming(requestInfo.getMetrics());

        cronetEngine.shutdown();
    }

    // Helper method to assert that the request is negotiated over QUIC.
    private void assertIsQuic(UrlResponseInfo responseInfo) {
        assertTrue(responseInfo.getNegotiatedProtocol().startsWith(QUIC_PROTOCOL_STRING_PREFIX));
    }
}
