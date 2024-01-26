// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.CronetTestRule.getTestStorage;

import android.os.StrictMode;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.MetricsTestUtil.TestExecutor;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

/** Test Network Quality Estimator. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason = "Fallback and AOSP implementations do not support network quality estimating")
public class NQETest {
    private static final String TAG = NQETest.class.getSimpleName();

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private String mUrl;

    // Thread on which network quality listeners should be notified.
    private Thread mNetworkQualityThread;

    @Before
    public void setUp() throws Exception {
        NativeTestServer.startNativeTestServer(mTestRule.getTestFramework().getContext());
        mUrl = NativeTestServer.getFileURL("/echo?status=200");
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    private class ExecutorThreadFactory implements ThreadFactory {
        @Override
        public Thread newThread(final Runnable r) {
            mNetworkQualityThread =
                    new Thread(
                            new Runnable() {
                                @Override
                                public void run() {
                                    StrictMode.ThreadPolicy threadPolicy =
                                            StrictMode.getThreadPolicy();
                                    try {
                                        StrictMode.setThreadPolicy(
                                                new StrictMode.ThreadPolicy.Builder()
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
    public void testNotEnabled() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        Executor networkQualityExecutor = Executors.newSingleThreadExecutor();
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(networkQualityExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(networkQualityExecutor);
        assertThrows(IllegalStateException.class, () -> cronetEngine.addRttListener(rttListener));
        assertThrows(
                IllegalStateException.class,
                () -> cronetEngine.addThroughputListener(throughputListener));
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();

        urlRequest.start();
        callback.blockForDone();
        assertThat(rttListener.rttObservationCount()).isEqualTo(0);
        assertThat(throughputListener.throughputObservationCount()).isEqualTo(0);
    }

    @Test
    @SmallTest
    public void testListenerRemoved() throws Exception {
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch((builder) -> builder.enableNetworkQualityEstimator(true));
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        TestExecutor networkQualityExecutor = new TestExecutor();
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(networkQualityExecutor);

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
        assertThat(rttListener.rttObservationCount()).isEqualTo(0);
    }

    // Returns whether a file contains a particular string.
    private boolean prefsFileContainsString(String content) throws IOException {
        File file =
                new File(
                        getTestStorage(mTestRule.getTestFramework().getContext())
                                + "/prefs/local_prefs.json");
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }

    @Test
    @SmallTest
    public void testQuicDisabled() throws Exception {
        UmaRecorderHolder.onLibraryLoaded(); // Hackish workaround to crbug.com/1338919
        assertThat(RttThroughputValues.INVALID_RTT_THROUGHPUT).isLessThan(0);
        Executor listenersExecutor = Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(listenersExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(listenersExecutor);
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableNetworkQualityEstimator(true)
                                    .enableHttp2(true)
                                    .enableQuic(false);

                            // The pref may not be written if the computed Effective Connection Type
                            // (ECT) matches the default ECT for the current connection type.
                            // Force the ECT to "Slow-2G". Since "Slow-2G" is not the default ECT
                            // for any connection type, this ensures that the pref is written to.
                            JSONObject nqeOptions =
                                    new JSONObject()
                                            .put("force_effective_connection_type", "Slow-2G");
                            JSONObject experimentalOptions =
                                    new JSONObject().put("NetworkQualityEstimator", nqeOptions);

                            builder.setExperimentalOptions(experimentalOptions.toString());
                            builder.setStoragePath(
                                    getTestStorage(mTestRule.getTestFramework().getContext()));
                        });

        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        cronetEngine.configureNetworkQualityEstimatorForTesting(true, true, true);

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

        assertThat(throughputListener.throughputObservationCount()).isGreaterThan(0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        assertThat(rttListener.rttObservationCount()).isGreaterThan(0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertThat(rttListener.rttObservationCount(0)).isGreaterThan(0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_TCP
        assertThat(rttListener.rttObservationCount(1)).isGreaterThan(0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertThat(rttListener.rttObservationCount(2)).isEqualTo(0);

        // Verify that the listeners were notified on the expected thread.
        assertThat(rttListener.getThread()).isEqualTo(mNetworkQualityThread);
        assertThat(throughputListener.getThread()).isEqualTo(mNetworkQualityThread);

        // Verify that effective connection type callback is received and
        // effective connection type is correctly set.
        assertThat(cronetEngine.getEffectiveConnectionType())
                .isNotEqualTo(EffectiveConnectionType.TYPE_UNKNOWN);

        // Verify that the HTTP RTT, transport RTT and downstream throughput
        // estimates are available.
        assertThat(cronetEngine.getHttpRttMs()).isAtLeast(0);
        assertThat(cronetEngine.getTransportRttMs()).isAtLeast(0);
        assertThat(cronetEngine.getDownstreamThroughputKbps()).isAtLeast(0);

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
        assertThat(prefsFileContainsString("network_qualities")).isTrue();

        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testPrefsWriteRead() throws Exception {
        // When the loop is run for the first time, network quality is written to the disk. The
        // test verifies that in the next loop, the network quality is read back.

        UmaRecorderHolder.onLibraryLoaded(); // Hackish workaround to crbug.com/1338919
        for (int i = 0; i <= 1; ++i) {

            // NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE: 3
            HistogramWatcher cachedRttHistogram =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord("NQE.RTT.ObservationSource", 3)
                            .allowExtraRecordsForHistogramsAbove()
                            .build();

            ExperimentalCronetEngine.Builder cronetEngineBuilder =
                    new ExperimentalCronetEngine.Builder(mTestRule.getTestFramework().getContext());
            assertThat(RttThroughputValues.INVALID_RTT_THROUGHPUT).isLessThan(0);
            Executor listenersExecutor =
                    Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
            TestNetworkQualityRttListener rttListener =
                    new TestNetworkQualityRttListener(listenersExecutor);
            cronetEngineBuilder
                    .enableNetworkQualityEstimator(true)
                    .enableHttp2(true)
                    .enableQuic(false);

            // The pref may not be written if the computed Effective Connection Type (ECT) matches
            // the default ECT for the current connection type. Force the ECT to "Slow-2G". Since
            // "Slow-2G" is not the default ECT for any connection type, this ensures that the pref
            // is written to.
            JSONObject nqeOptions =
                    new JSONObject().put("force_effective_connection_type", "Slow-2G");
            JSONObject experimentalOptions =
                    new JSONObject().put("NetworkQualityEstimator", nqeOptions);

            cronetEngineBuilder.setExperimentalOptions(experimentalOptions.toString());

            cronetEngineBuilder.setStoragePath(
                    getTestStorage(mTestRule.getTestFramework().getContext()));

            final ExperimentalCronetEngine cronetEngine = cronetEngineBuilder.build();
            cronetEngine.configureNetworkQualityEstimatorForTesting(true, true, true);
            cronetEngine.addRttListener(rttListener);

            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder builder =
                    cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
            UrlRequest urlRequest = builder.build();
            urlRequest.start();
            callback.blockForDone();

            // Wait for RTT observation (at the URL request layer) to be posted.
            rttListener.waitUntilFirstUrlRequestRTTReceived();

            // Check RTT observation count after throughput observation has been received. This
            // ensures that executor has finished posting the RTT observation to the RTT
            // listeners.
            assertThat(rttListener.rttObservationCount()).isGreaterThan(0);

            // Verify that effective connection type callback is received and
            // effective connection type is correctly set.
            assertThat(cronetEngine.getEffectiveConnectionType())
                    .isNotEqualTo(EffectiveConnectionType.TYPE_UNKNOWN);

            cronetEngine.shutdown();

            if (i == 0) {
                // Verify that the cached estimates were written to the prefs.
                assertThat(prefsFileContainsString("network_qualities")).isTrue();
            }

            if (i > 0) {
                cachedRttHistogram.assertExpected();
            }
        }
    }

    @Test
    @SmallTest
    public void testQuicDisabledWithParams() throws Exception {
        Executor listenersExecutor = Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(listenersExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(listenersExecutor);

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            // Force the effective connection type to "2G".
                            JSONObject nqeOptions =
                                    new JSONObject()
                                            .put("force_effective_connection_type", "Slow-2G");
                            // Add one more extra param two times to ensure robustness.
                            nqeOptions.put("some_other_param_1", "value1");
                            nqeOptions.put("some_other_param_2", "value2");
                            JSONObject experimentalOptions =
                                    new JSONObject().put("NetworkQualityEstimator", nqeOptions);
                            experimentalOptions.put("SomeOtherFieldTrialName", new JSONObject());

                            builder.enableNetworkQualityEstimator(true)
                                    .enableHttp2(true)
                                    .enableQuic(false);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                        });

        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

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

        assertThat(throughputListener.throughputObservationCount()).isGreaterThan(0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        assertThat(rttListener.rttObservationCount()).isGreaterThan(0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertThat(rttListener.rttObservationCount(0)).isGreaterThan(0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_TCP
        assertThat(rttListener.rttObservationCount(1)).isGreaterThan(0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertThat(rttListener.rttObservationCount(2)).isEqualTo(0);

        // Verify that the listeners were notified on the expected thread.
        assertThat(rttListener.getThread()).isEqualTo(mNetworkQualityThread);
        assertThat(throughputListener.getThread()).isEqualTo(mNetworkQualityThread);

        // Verify that effective connection type callback is received and effective connection type
        // is correctly set to the forced value. This also verifies that the configuration params
        // from Cronet embedders were correctly read by NetworkQualityEstimator.
        assertThat(cronetEngine.getEffectiveConnectionType())
                .isEqualTo(EffectiveConnectionType.TYPE_SLOW_2G);
    }
}
