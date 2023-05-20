// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static android.os.Process.THREAD_PRIORITY_BACKGROUND;
import static android.os.Process.THREAD_PRIORITY_DEFAULT;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP;

import android.content.Context;
import android.os.Build;
import android.os.ConditionVariable;

import androidx.test.filters.SmallTest;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetLoggerTestRule;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.NativeTestServer;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UrlRequest;
import org.chromium.net.impl.CronetEngineBuilderImpl.HttpCacheMode;
import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.CronetLogger.CronetVersion;

import java.time.Duration;
import java.util.AbstractMap;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test logging functionalities.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(JUnit4.class)
@RequiresMinAndroidApi(Build.VERSION_CODES.O)
public final class CronetLoggerTest {
    private final CronetTestRule mTestRule = new CronetTestRule();
    private final CronetLoggerTestRule mLoggerTestRule = new CronetLoggerTestRule(TestLogger.class);

    @Rule
    public final RuleChain chain = RuleChain.outerRule(mTestRule).around(mLoggerTestRule);

    private TestLogger mTestLogger;
    private Context mContext;
    private CronetTestFramework mTestFramework;

    /**
     * Records the last engine creation (and traffic info) call it has received.
     */
    public static final class TestLogger extends CronetLogger {
        private AtomicInteger mCallsToLogCronetEngineCreation = new AtomicInteger();
        private AtomicInteger mCallsToLogCronetTrafficInfo = new AtomicInteger();
        private AtomicInteger mCronetEngineId = new AtomicInteger();
        private AtomicInteger mCronetRequestId = new AtomicInteger();
        private AtomicReference<CronetTrafficInfo> mTrafficInfo = new AtomicReference<>();
        private AtomicReference<CronetEngineBuilderInfo> mBuilderInfo = new AtomicReference<>();
        private AtomicReference<CronetVersion> mVersion = new AtomicReference<>();
        private AtomicReference<CronetSource> mSource = new AtomicReference<>();
        private final ConditionVariable mBlock = new ConditionVariable();

        @Override
        public void logCronetEngineCreation(int cronetEngineId,
                CronetEngineBuilderInfo engineBuilderInfo, CronetVersion version,
                CronetSource source) {
            mCallsToLogCronetEngineCreation.incrementAndGet();
            mCronetEngineId.set(cronetEngineId);
            mBuilderInfo.set(engineBuilderInfo);
            mVersion.set(version);
            mSource.set(source);
        }

        @Override
        public void logCronetTrafficInfo(int cronetEngineId, CronetTrafficInfo trafficInfo) {
            mCallsToLogCronetTrafficInfo.incrementAndGet();
            mCronetRequestId.set(cronetEngineId);
            mTrafficInfo.set(trafficInfo);
            mBlock.open();
        }

        public int callsToLogCronetTrafficInfo() {
            return mCallsToLogCronetTrafficInfo.get();
        }

        public int callsToLogCronetEngineCreation() {
            return mCallsToLogCronetEngineCreation.get();
        }

        public void waitForLogCronetTrafficInfo() {
            mBlock.block();
            mBlock.close();
        }

        public int getLastCronetEngineId() {
            return mCronetEngineId.get();
        }

        public int getLastCronetRequestId() {
            return mCronetRequestId.get();
        }

        public CronetTrafficInfo getLastCronetTrafficInfo() {
            return mTrafficInfo.get();
        }

        public CronetEngineBuilderInfo getLastCronetEngineBuilderInfo() {
            return mBuilderInfo.get();
        }

        public CronetVersion getLastCronetVersion() {
            return mVersion.get();
        }

        public CronetSource getLastCronetSource() {
            return mSource.get();
        }
    }

    @Before
    public void setUp() {
        mContext = CronetTestRule.getContext();
        mTestFramework = mTestRule.buildCronetTestFramework();
        mTestLogger = (TestLogger) mLoggerTestRule.mTestLogger;
        assertTrue(NativeTestServer.startNativeTestServer(mContext));
    }

    @After
    public void tearDown() {
        mTestLogger = null;
        NativeTestServer.shutdownNativeTestServer();
        mTestFramework.shutdownEngine();
    }

    @Test
    @SmallTest
    public void testCronetEngineInfoCreation() {
        CronetEngineBuilderImpl builder = new NativeCronetEngineBuilderImpl(mContext);
        CronetEngineBuilderInfo builderInfo = new CronetEngineBuilderInfo(builder);
        assertThat(builderInfo.isPublicKeyPinningBypassForLocalTrustAnchorsEnabled())
                .isEqualTo(builder.publicKeyPinningBypassForLocalTrustAnchorsEnabled());
        assertThat(builderInfo.getUserAgent()).isEqualTo(builder.getUserAgent());
        assertThat(builderInfo.getStoragePath()).isEqualTo(builder.storagePath());
        assertThat(builderInfo.isQuicEnabled()).isEqualTo(builder.quicEnabled());
        assertThat(builderInfo.isHttp2Enabled()).isEqualTo(builder.http2Enabled());
        assertThat(builderInfo.isBrotliEnabled()).isEqualTo(builder.brotliEnabled());
        assertThat(builderInfo.getHttpCacheMode()).isEqualTo(builder.publicBuilderHttpCacheMode());
        assertThat(builderInfo.getExperimentalOptions()).isEqualTo(builder.experimentalOptions());
        assertThat(builderInfo.isNetworkQualityEstimatorEnabled())
                .isEqualTo(builder.networkQualityEstimatorEnabled());
        assertThat(builderInfo.getThreadPriority())
                .isEqualTo(builder.threadPriority(THREAD_PRIORITY_BACKGROUND));
    }

    @Test
    @SmallTest
    public void testCronetVersionCreation() {
        final int major = 100;
        final int minor = 0;
        final int build = 1;
        final int patch = 33;
        final String version = String.format(Locale.US, "%d.%d.%d.%d", major, minor, build, patch);
        final CronetVersion parsedVersion = new CronetVersion(version);
        assertThat(parsedVersion.getMajorVersion()).isEqualTo(major);
        assertThat(parsedVersion.getMinorVersion()).isEqualTo(minor);
        assertThat(parsedVersion.getBuildVersion()).isEqualTo(build);
        assertThat(parsedVersion.getPatchVersion()).isEqualTo(patch);
    }

    @Test
    @SmallTest
    public void testHttpCacheModeEnum() {
        final int publicBuilderHttpCacheModes[] = {CronetEngine.Builder.HTTP_CACHE_DISABLED,
                CronetEngine.Builder.HTTP_CACHE_IN_MEMORY,
                CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP, CronetEngine.Builder.HTTP_CACHE_DISK};
        for (int publicBuilderHttpCacheMode : publicBuilderHttpCacheModes) {
            HttpCacheMode cacheModeEnum =
                    HttpCacheMode.fromPublicBuilderCacheMode(publicBuilderHttpCacheMode);
            assertThat(cacheModeEnum.toPublicBuilderCacheMode())
                    .isEqualTo(publicBuilderHttpCacheMode);
        }
    }

    @Test
    @SmallTest
    public void testSetLoggerForTesting() {
        CronetLogger logger = CronetLoggerFactory.createLogger(mContext, null);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(0);
        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(0);

        // We don't care about what's being logged.
        logger.logCronetTrafficInfo(0, null);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(1);
        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(0);
        logger.logCronetEngineCreation(0, null, null, null);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(1);
        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(1);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testTelemetryDefaultDisabled() throws JSONException {
        final String url = NativeTestServer.getEchoBodyURL();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        CronetEngine engine = mTestFramework.startEngine();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest request = requestBuilder.build();
        request.start();
        callback.blockForDone();

        // Test-logger should be bypassed.
        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(0);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(0);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testEngineCreation() throws JSONException {
        JSONObject staleDns = new JSONObject()
                                      .put("enable", true)
                                      .put("delay_ms", 0)
                                      .put("allow_other_network", true)
                                      .put("persist_to_disk", true)
                                      .put("persist_delay_ms", 0);
        final JSONObject jsonExperimentalOptions =
                new JSONObject().put("StaleDNS", staleDns).put("enable_telemetry", true);
        final String experimentalOptions = jsonExperimentalOptions.toString();
        final boolean isPublicKeyPinningBypassForLocalTrustAnchorsEnabled = false;
        final String userAgent = "myUserAgent";
        final String storagePath = CronetTestRule.getTestStorage(mContext);
        final boolean isQuicEnabled = true;
        final boolean isHttp2Enabled = false;
        final boolean isBrotliEnabled = true;
        final int cacheMode = HTTP_CACHE_DISK_NO_HTTP;
        final boolean isNetworkQualityEstimatorEnabled = true;
        final int threadPriority = THREAD_PRIORITY_DEFAULT;

        ExperimentalCronetEngine.Builder builder =
                (ExperimentalCronetEngine.Builder) mTestFramework.mBuilder;

        builder.setExperimentalOptions(experimentalOptions);
        builder.enablePublicKeyPinningBypassForLocalTrustAnchors(
                isPublicKeyPinningBypassForLocalTrustAnchorsEnabled);
        builder.setUserAgent(userAgent);
        builder.setStoragePath(storagePath);
        builder.enableQuic(isQuicEnabled);
        builder.enableHttp2(isHttp2Enabled);
        builder.enableBrotli(isBrotliEnabled);
        builder.enableHttpCache(cacheMode, 0);
        builder.enableNetworkQualityEstimator(isNetworkQualityEstimatorEnabled);
        builder.setThreadPriority(threadPriority);

        CronetEngine engine = mTestFramework.startEngine();
        final CronetEngineBuilderInfo builderInfo = mTestLogger.getLastCronetEngineBuilderInfo();
        final CronetVersion version = mTestLogger.getLastCronetVersion();
        final CronetSource source = mTestLogger.getLastCronetSource();

        assertThat(builderInfo.isPublicKeyPinningBypassForLocalTrustAnchorsEnabled())
                .isEqualTo(isPublicKeyPinningBypassForLocalTrustAnchorsEnabled);
        assertThat(builderInfo.getUserAgent()).isEqualTo(userAgent);
        assertThat(builderInfo.getStoragePath()).isEqualTo(storagePath);
        assertThat(builderInfo.isQuicEnabled()).isEqualTo(isQuicEnabled);
        assertThat(builderInfo.isHttp2Enabled()).isEqualTo(isHttp2Enabled);
        assertThat(builderInfo.isBrotliEnabled()).isEqualTo(isBrotliEnabled);
        assertThat(builderInfo.getHttpCacheMode()).isEqualTo(cacheMode);
        assertThat(builderInfo.getExperimentalOptions()).isEqualTo(experimentalOptions);
        assertThat(builderInfo.isNetworkQualityEstimatorEnabled())
                .isEqualTo(isNetworkQualityEstimatorEnabled);
        assertThat(builderInfo.getThreadPriority()).isEqualTo(threadPriority);
        assertThat(version.toString()).isEqualTo(ImplVersion.getCronetVersion());
        if (mTestRule.testingJavaImpl()) {
            assertThat(source).isEqualTo(CronetSource.CRONET_SOURCE_FALLBACK);
        } else {
            assertThat(source).isEqualTo(CronetSource.CRONET_SOURCE_STATICALLY_LINKED);
        }

        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(1);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(0);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testEngineCreationAndTrafficInfoEngineId() throws Exception {
        JSONObject jsonExperimentalOptions = new JSONObject().put("enable_telemetry", true);
        final String experimentalOptions = jsonExperimentalOptions.toString();
        final String url = "www.example.com";
        ExperimentalCronetEngine.Builder builder =
                (ExperimentalCronetEngine.Builder) mTestFramework.mBuilder;
        builder.setExperimentalOptions(experimentalOptions);
        CronetEngine engine = mTestFramework.startEngine();
        final int engineId = mTestLogger.getLastCronetEngineId();

        TestUrlRequestCallback callback1 = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder1 =
                engine.newUrlRequestBuilder(url, callback1, callback1.getExecutor());
        UrlRequest request1 = requestBuilder1.build();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder2 =
                engine.newUrlRequestBuilder(url, callback2, callback2.getExecutor());
        UrlRequest request2 = requestBuilder2.build();

        request1.start();
        callback1.blockForDone();
        mTestLogger.waitForLogCronetTrafficInfo();
        final int request1Id = mTestLogger.getLastCronetRequestId();

        request2.start();
        callback2.blockForDone();
        mTestLogger.waitForLogCronetTrafficInfo();
        final int request2Id = mTestLogger.getLastCronetRequestId();

        assertThat(request1Id).isEqualTo(engineId);
        assertThat(request2Id).isEqualTo(engineId);

        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(1);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(2);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testMultipleEngineCreationAndTrafficInfoEngineId() throws Exception {
        JSONObject jsonExperimentalOptions = new JSONObject().put("enable_telemetry", true);
        final String experimentalOptions = jsonExperimentalOptions.toString();
        final String url = "www.example.com";
        ExperimentalCronetEngine.Builder engineBuilder =
                (ExperimentalCronetEngine.Builder) mTestFramework.mBuilder;
        engineBuilder.setExperimentalOptions(experimentalOptions);

        CronetEngine engine1 = engineBuilder.build();
        final int engine1Id = mTestLogger.getLastCronetEngineId();
        CronetEngine engine2 = engineBuilder.build();
        final int engine2Id = mTestLogger.getLastCronetEngineId();

        TestUrlRequestCallback callback1 = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder1 =
                engine1.newUrlRequestBuilder(url, callback1, callback1.getExecutor());
        UrlRequest request1 = requestBuilder1.build();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder2 =
                engine2.newUrlRequestBuilder(url, callback2, callback2.getExecutor());
        UrlRequest request2 = requestBuilder2.build();

        request1.start();
        callback1.blockForDone();
        mTestLogger.waitForLogCronetTrafficInfo();
        final int request1Id = mTestLogger.getLastCronetRequestId();

        request2.start();
        callback2.blockForDone();
        mTestLogger.waitForLogCronetTrafficInfo();
        final int request2Id = mTestLogger.getLastCronetRequestId();

        assertThat(request1Id).isEqualTo(engine1Id);
        assertThat(request2Id).isEqualTo(engine2Id);

        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(2);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(2);

        engine1.shutdown();
        engine2.shutdown();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSuccessfulRequestNative() throws Exception {
        JSONObject jsonExperimentalOptions = new JSONObject().put("enable_telemetry", true);
        final String experimentalOptions = jsonExperimentalOptions.toString();
        final String url = NativeTestServer.getEchoBodyURL();
        ExperimentalCronetEngine.Builder engineBuilder =
                (ExperimentalCronetEngine.Builder) mTestFramework.mBuilder;
        engineBuilder.setExperimentalOptions(experimentalOptions);
        CronetEngine engine = mTestFramework.startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest request = requestBuilder.build();
        request.start();
        callback.blockForDone();
        assertFalse(callback.mOnCanceledCalled);
        assertFalse(callback.mOnErrorCalled);
        mTestLogger.waitForLogCronetTrafficInfo();

        final CronetTrafficInfo trafficInfo = mTestLogger.getLastCronetTrafficInfo();
        assertThat(trafficInfo.getRequestHeaderSizeInBytes()).isEqualTo(0);
        assertNotEquals(0, trafficInfo.getRequestBodySizeInBytes());
        assertNotEquals(0, trafficInfo.getResponseHeaderSizeInBytes());
        assertNotEquals(0, trafficInfo.getResponseBodySizeInBytes());
        assertThat(trafficInfo.getResponseStatusCode()).isEqualTo(200);
        assertNotEquals(Duration.ofSeconds(0), trafficInfo.getHeadersLatency());
        assertNotEquals(Duration.ofSeconds(0), trafficInfo.getTotalLatency());
        assertNotNull(trafficInfo.getNegotiatedProtocol());
        assertFalse(trafficInfo.wasConnectionMigrationAttempted());
        assertFalse(trafficInfo.didConnectionMigrationSucceed());

        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(1);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(1);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testFailedRequestNative() throws Exception {
        JSONObject jsonExperimentalOptions = new JSONObject().put("enable_telemetry", true);
        final String url = "www.unreachable-url.com";
        final String experimentalOptions = jsonExperimentalOptions.toString();
        ExperimentalCronetEngine.Builder engineBuilder =
                (ExperimentalCronetEngine.Builder) mTestFramework.mBuilder;
        engineBuilder.setExperimentalOptions(experimentalOptions);
        CronetEngine engine = mTestFramework.startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest request = requestBuilder.build();
        request.start();
        callback.blockForDone();
        assertFalse(callback.mOnCanceledCalled);
        assertTrue(callback.mOnErrorCalled);
        mTestLogger.waitForLogCronetTrafficInfo();

        final CronetTrafficInfo trafficInfo = mTestLogger.getLastCronetTrafficInfo();
        assertThat(trafficInfo.getRequestHeaderSizeInBytes()).isEqualTo(0);
        assertThat(trafficInfo.getRequestBodySizeInBytes()).isEqualTo(0);
        assertThat(trafficInfo.getResponseHeaderSizeInBytes()).isEqualTo(0);
        assertThat(trafficInfo.getResponseBodySizeInBytes()).isEqualTo(0);
        // When a request fails before hitting the server all these values won't be populated in
        // the actual code. Check that the logger sets them to some known defaults before
        // logging.
        assertThat(trafficInfo.getResponseStatusCode()).isEqualTo(0);
        assertThat(trafficInfo.getNegotiatedProtocol()).isEmpty();
        assertFalse(trafficInfo.wasConnectionMigrationAttempted());
        assertFalse(trafficInfo.didConnectionMigrationSucceed());

        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(1);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(1);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCanceledRequestNative() throws Exception {
        JSONObject jsonExperimentalOptions = new JSONObject().put("enable_telemetry", true);
        final String experimentalOptions = jsonExperimentalOptions.toString();
        final String url = NativeTestServer.getEchoBodyURL();
        ExperimentalCronetEngine.Builder engineBuilder =
                (ExperimentalCronetEngine.Builder) mTestFramework.mBuilder;
        engineBuilder.setExperimentalOptions(experimentalOptions);
        CronetEngine engine = mTestFramework.startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest request = requestBuilder.build();
        request.start();
        request.cancel();
        callback.blockForDone();
        assertTrue(callback.mOnCanceledCalled);
        assertFalse(callback.mOnErrorCalled);
        mTestLogger.waitForLogCronetTrafficInfo();

        final CronetTrafficInfo trafficInfo = mTestLogger.getLastCronetTrafficInfo();
        assertThat(trafficInfo.getRequestHeaderSizeInBytes()).isEqualTo(0);
        assertThat(trafficInfo.getRequestBodySizeInBytes()).isEqualTo(0);
        assertThat(trafficInfo.getResponseHeaderSizeInBytes()).isEqualTo(0);
        assertThat(trafficInfo.getResponseBodySizeInBytes()).isEqualTo(0);
        // When a request fails before hitting the server all these values won't be populated in
        // the actual code. Check that the logger sets them to some known defaults before
        // logging.
        assertThat(trafficInfo.getResponseStatusCode()).isEqualTo(0);
        assertThat(trafficInfo.getNegotiatedProtocol()).isEmpty();
        assertFalse(trafficInfo.wasConnectionMigrationAttempted());
        assertFalse(trafficInfo.didConnectionMigrationSucceed());

        assertThat(mTestLogger.callsToLogCronetEngineCreation()).isEqualTo(1);
        assertThat(mTestLogger.callsToLogCronetTrafficInfo()).isEqualTo(1);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testEmptyHeadersSizeNative() {
        Map<String, List<String>> headers = Collections.emptyMap();
        assertThat(CronetUrlRequest.estimateHeadersSizeInBytes(headers)).isEqualTo(0);
        headers = null;
        assertThat(CronetUrlRequest.estimateHeadersSizeInBytes(headers)).isEqualTo(0);

        CronetUrlRequest.HeadersList headersList = new CronetUrlRequest.HeadersList();
        assertThat(CronetUrlRequest.estimateHeadersSizeInBytes(headersList)).isEqualTo(0);
        headersList = null;
        assertThat(CronetUrlRequest.estimateHeadersSizeInBytes(headersList)).isEqualTo(0);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testNonEmptyHeadersSizeNative() {
        Map<String, List<String>> headers = new HashMap<String, List<String>>() {
            {
                put("header1", Arrays.asList("value1", "value2")); // 7 + 6 + 6 = 19
                put("header2", null); // 19 + 7 = 26
                put("header3", Collections.emptyList()); // 26 + 7 + 0 = 33
                put(null, Arrays.asList("")); // 33 + 0 + 0 = 33
            }
        };
        assertThat(CronetUrlRequest.estimateHeadersSizeInBytes(headers)).isEqualTo(33);

        CronetUrlRequest.HeadersList headersList = new CronetUrlRequest.HeadersList();
        headersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                "header1", "value1") // 7 + 6 = 13
        );
        headersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                "header1", "value2") // 13 + 7 + 6 = 26
        );
        headersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                "header2", null) // 26 + 7 + 0 = 33
        );
        headersList.add(
                new AbstractMap.SimpleImmutableEntry<String, String>(null, "") // 33 + 0 + 0 = 33
        );
        assertThat(CronetUrlRequest.estimateHeadersSizeInBytes(headersList)).isEqualTo(33);
    }
}
