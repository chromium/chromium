// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.getContext;
import static org.chromium.net.CronetTestRule.getTestStorage;

import android.os.StrictMode;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.MetricsTestUtil.TestExecutor;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

/**
 * Test Network Quality Estimator.
 */
@RunWith(AndroidJUnit4.class)
public class NQETest {
    private static final String TAG = NQETest.class.getSimpleName();

    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private EmbeddedTestServer mTestServer;
    private String mUrl;

    // Thread on which network quality listeners should be notified.
    private Thread mNetworkQualityThread;

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getContext());
        mUrl = mTestServer.getURL("/echo?status=200");
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private class ExecutorThreadFactory implements ThreadFactory {
        @Override
        public Thread newThread(final Runnable r) {
            mNetworkQualityThread = new Thread(new Runnable() {
                @Override
                public void run() {
                    StrictMode.ThreadPolicy threadPolicy = StrictMode.getThreadPolicy();
                    try {
                        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                                           .detectNetwork()
                                                           .penaltyLog()
                                                           .penaltyDeath()
                                                           .build());
                        r.run();
                    } finally {
                        StrictMode.setThreadPolicy(threadPolicy);
                    }
                }
            });
            return mNetworkQualityThread;
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testNotEnabled() throws Exception {
        ExperimentalCronetEngine.Builder cronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        final ExperimentalCronetEngine cronetEngine = cronetEngineBuilder.build();
        Executor networkQualityExecutor = Executors.newSingleThreadExecutor();
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(networkQualityExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(networkQualityExecutor);
        try {
            cronetEngine.addRttListener(rttListener);
            fail("Should throw an exception.");
        } catch (IllegalStateException e) {
        }
        try {
            cronetEngine.addThroughputListener(throughputListener);
            fail("Should throw an exception.");
        } catch (IllegalStateException e) {
        }
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();

        urlRequest.start();
        callback.blockForDone();
        assertEquals(0, rttListener.rttObservationCount());
        assertEquals(0, throughputListener.throughputObservationCount());
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testListenerRemoved() throws Exception {
        ExperimentalCronetEngine.Builder cronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        TestExecutor networkQualityExecutor = new TestExecutor();
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(networkQualityExecutor);
        cronetEngineBuilder.enableNetworkQualityEstimator(true);
        final ExperimentalCronetEngine cronetEngine = cronetEngineBuilder.build();
        cronetEngine.configureNetworkQualityEstimatorForTesting(true, true, false);

        cronetEngine.addRttListener(rttListener);
        cronetEngine.removeRttListener(rttListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        networkQualityExecutor.runAllTasks();
        assertEquals(0, rttListener.rttObservationCount());
        cronetEngine.shutdown();
    }

    // Returns whether a file contains a particular string.
    private boolean prefsFileContainsString(String content) throws IOException {
        File file = new File(getTestStorage(getContext()) + "/prefs/local_prefs.json");
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    @DisabledTest(message = "crbug.com/796260")
    public void testQuicDisabled() throws Exception {
        ExperimentalCronetEngine.Builder cronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        assertTrue(RttThroughputValues.INVALID_RTT_THROUGHPUT < 0);
        Executor listenersExecutor = Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(listenersExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(listenersExecutor);
        cronetEngineBuilder.enableNetworkQualityEstimator(true).enableHttp2(true).enableQuic(false);

        // The pref may not be written if the computed Effective Connection Type (ECT) matches the
        // default ECT for the current connection type. Force the ECT to "Slow-2G". Since "Slow-2G"
        // is not the default ECT for any connection type, this ensures that the pref is written to.
        JSONObject nqeOptions = new JSONObject().put("force_effective_connection_type", "Slow-2G");
        JSONObject experimentalOptions =
                new JSONObject().put("NetworkQualityEstimator", nqeOptions);

        cronetEngineBuilder.setExperimentalOptions(experimentalOptions.toString());

        cronetEngineBuilder.setStoragePath(getTestStorage(getContext()));
        final ExperimentalCronetEngine cronetEngine = cronetEngineBuilder.build();
        cronetEngine.configureNetworkQualityEstimatorForTesting(true, true, true);

        cronetEngine.addRttListener(rttListener);
        cronetEngine.addThroughputListener(throughputListener);

        HistogramDelta writeCountHistogram = new HistogramDelta("NQE.Prefs.WriteCount", 1);
        assertEquals(0, writeCountHistogram.getDelta()); // Sanity check.

        HistogramDelta readCountHistogram = new HistogramDelta("NQE.Prefs.ReadCount", 1);
        assertEquals(0, readCountHistogram.getDelta()); // Sanity check.

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();

        // Throughput observation is posted to the network quality estimator on the network thread
        // after the UrlRequest is completed. The observations are then eventually posted to
        // throughput listeners on the executor provided to network quality.
        throughputListener.waitUntilFirstThroughputObservationReceived();

        // Wait for RTT observation (at the URL request layer) to be posted.
        rttListener.waitUntilFirstUrlRequestRTTReceived();

        assertTrue(throughputListener.throughputObservationCount() > 0);

        // Prefs must be read at startup.
        assertTrue(readCountHistogram.getDelta() > 0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        assertTrue(rttListener.rttObservationCount() > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertTrue(rttListener.rttObservationCount(0) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_TCP
        assertTrue(rttListener.rttObservationCount(1) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertEquals(0, rttListener.rttObservationCount(2));

        // Verify that the listeners were notified on the expected thread.
        assertEquals(mNetworkQualityThread, rttListener.getThread());
        assertEquals(mNetworkQualityThread, throughputListener.getThread());

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
            Thread.sleep(12000);
            try {
                if (prefsFileContainsString("network_qualities")) {
                    break;
                }
            } catch (FileNotFoundException e) {
                // Ignored this exception since the file will only be created when updates are
                // flushed to the disk.
            }
        }
        assertTrue(prefsFileContainsString("network_qualities"));

        cronetEngine.shutdown();
        assertTrue(writeCountHistogram.getDelta() > 0);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @Feature({"Cronet"})
    public void testPrefsWriteRead() throws Exception {
        // When the loop is run for the first time, network quality is written to the disk. The
        // test verifies that in the next loop, the network quality is read back.
        for (int i = 0; i <= 1; ++i) {
            ExperimentalCronetEngine.Builder cronetEngineBuilder =
                    new ExperimentalCronetEngine.Builder(getContext());
            assertTrue(RttThroughputValues.INVALID_RTT_THROUGHPUT < 0);
            Executor listenersExecutor =
                    Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
            TestNetworkQualityRttListener rttListener =
                    new TestNetworkQualityRttListener(listenersExecutor);
            cronetEngineBuilder.enableNetworkQualityEstimator(true).enableHttp2(true).enableQuic(
                    false);

            // The pref may not be written if the computed Effective Connection Type (ECT) matches
            // the default ECT for the current connection type. Force the ECT to "Slow-2G". Since
            // "Slow-2G" is not the default ECT for any connection type, this ensures that the pref
            // is written to.
            JSONObject nqeOptions =
                    new JSONObject().put("force_effective_connection_type", "Slow-2G");
            JSONObject experimentalOptions =
                    new JSONObject().put("NetworkQualityEstimator", nqeOptions);

            cronetEngineBuilder.setExperimentalOptions(experimentalOptions.toString());

            cronetEngineBuilder.setStoragePath(getTestStorage(getContext()));

            final ExperimentalCronetEngine cronetEngine = cronetEngineBuilder.build();
            cronetEngine.configureNetworkQualityEstimatorForTesting(true, true, true);
            cronetEngine.addRttListener(rttListener);

            HistogramDelta writeCountHistogram = new HistogramDelta("NQE.Prefs.WriteCount", 1);
            assertEquals(0, writeCountHistogram.getDelta()); // Sanity check.

            HistogramDelta readCountHistogram = new HistogramDelta("NQE.Prefs.ReadCount", 1);
            assertEquals(0, readCountHistogram.getDelta()); // Sanity check.

            HistogramDelta readPrefsSizeHistogram = new HistogramDelta("NQE.Prefs.ReadSize", 1);
            assertEquals(0, readPrefsSizeHistogram.getDelta()); // Sanity check.

            // NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE: 3
            HistogramDelta cachedRttHistogram = new HistogramDelta("NQE.RTT.ObservationSource", 3);
            assertEquals(0, cachedRttHistogram.getDelta()); // Sanity check.

            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder builder =
                    cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
            UrlRequest urlRequest = builder.build();
            urlRequest.start();
            callback.blockForDone();

            // Wait for RTT observation (at the URL request layer) to be posted.
            rttListener.waitUntilFirstUrlRequestRTTReceived();

            // Prefs must be read at startup.
            assertTrue(readCountHistogram.getDelta() > 0);

            // Check RTT observation count after throughput observation has been received. This
            // ensures
            // that executor has finished posting the RTT observation to the RTT listeners.
            assertTrue(rttListener.rttObservationCount() > 0);

            // Verify that effective connection type callback is received and
            // effective connection type is correctly set.
            assertTrue(cronetEngine.getEffectiveConnectionType()
                    != EffectiveConnectionType.TYPE_UNKNOWN);

            cronetEngine.shutdown();

            if (i == 0) {
                // Verify that the cached estimates were written to the prefs.
                assertTrue(prefsFileContainsString("network_qualities"));
            }

            // Stored network quality in the pref should be read in the second iteration.
            assertEquals(readPrefsSizeHistogram.getDelta() > 0, i > 0);
            if (i > 0) {
                assertTrue(cachedRttHistogram.getDelta() > 0);
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    @DisabledTest(message = "crbug.com/796260")
    public void testQuicDisabledWithParams() throws Exception {
        ExperimentalCronetEngine.Builder cronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        Executor listenersExecutor = Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(listenersExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(listenersExecutor);

        // Force the effective connection type to "2G".
        JSONObject nqeOptions = new JSONObject().put("force_effective_connection_type", "Slow-2G");
        // Add one more extra param two times to ensure robustness.
        nqeOptions.put("some_other_param_1", "value1");
        nqeOptions.put("some_other_param_2", "value2");
        JSONObject experimentalOptions =
                new JSONObject().put("NetworkQualityEstimator", nqeOptions);
        experimentalOptions.put("SomeOtherFieldTrialName", new JSONObject());

        cronetEngineBuilder.enableNetworkQualityEstimator(true).enableHttp2(true).enableQuic(false);
        cronetEngineBuilder.setExperimentalOptions(experimentalOptions.toString());
        final ExperimentalCronetEngine cronetEngine = cronetEngineBuilder.build();
        cronetEngine.configureNetworkQualityEstimatorForTesting(true, true, false);

        cronetEngine.addRttListener(rttListener);
        cronetEngine.addThroughputListener(throughputListener);

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();

        // Throughput observation is posted to the network quality estimator on the network thread
        // after the UrlRequest is completed. The observations are then eventually posted to
        // throughput listeners on the executor provided to network quality.
        throughputListener.waitUntilFirstThroughputObservationReceived();

        // Wait for RTT observation (at the URL request layer) to be posted.
        rttListener.waitUntilFirstUrlRequestRTTReceived();

        assertTrue(throughputListener.throughputObservationCount() > 0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        assertTrue(rttListener.rttObservationCount() > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertTrue(rttListener.rttObservationCount(0) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_TCP
        assertTrue(rttListener.rttObservationCount(1) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertEquals(0, rttListener.rttObservationCount(2));

        // Verify that the listeners were notified on the expected thread.
        assertEquals(mNetworkQualityThread, rttListener.getThread());
        assertEquals(mNetworkQualityThread, throughputListener.getThread());

        // Verify that effective connection type callback is received and effective connection type
        // is correctly set to the forced value. This also verifies that the configuration params
        // from Cronet embedders were correctly read by NetworkQualityEstimator.
        assertEquals(
                EffectiveConnectionType.TYPE_SLOW_2G, cronetEngine.getEffectiveConnectionType());

        cronetEngine.shutdown();
    }
}
