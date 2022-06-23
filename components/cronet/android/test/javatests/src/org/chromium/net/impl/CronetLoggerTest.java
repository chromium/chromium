// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static android.os.Process.THREAD_PRIORITY_BACKGROUND;
import static android.os.Process.THREAD_PRIORITY_DEFAULT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP;

import android.content.Context;
import android.os.Build;
import android.os.ConditionVariable;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.CronetTestRule;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.NativeTestServer;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UrlRequest;
import org.chromium.net.impl.CronetEngineBuilderImpl.HttpCacheMode;
import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
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
@RunWith(JUnit4.class)
public final class CronetLoggerTest {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";
    private TestLogger mTestLogger;
    private Context mContext;
    private CronetProvider mNativeProvider;

    final class TestLogger extends CronetLogger {
        private AtomicInteger mCallsToLogCronetEngineCreation = new AtomicInteger();
        private AtomicInteger mCallsToLogCronetTrafficInfo = new AtomicInteger();
        private AtomicInteger mCronetEngineId = new AtomicInteger();
        private AtomicInteger mCronetRequestId = new AtomicInteger();
        private AtomicReference<CronetTrafficInfo> mTrafficInfo = new AtomicReference<>();
        private AtomicReference<CronetEngineBuilderInfo> mBuilderInfo = new AtomicReference<>();
        private AtomicReference<CronetVersion> mVersion = new AtomicReference<>();
        private final ConditionVariable mBlock = new ConditionVariable();

        @Override
        public void logCronetEngineCreation(int cronetEngineId,
                CronetEngineBuilderInfo engineBuilderInfo, CronetVersion version,
                CronetSource source) {
            mCallsToLogCronetEngineCreation.incrementAndGet();
            mCronetEngineId.set(cronetEngineId);
            mBuilderInfo.set(engineBuilderInfo);
            mVersion.set(version);
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
    }

    @Before
    public void setUp() {
        System.loadLibrary("cronet_tests");
        mTestLogger = new TestLogger();
        CronetLoggerFactory.setLoggerForTesting(mTestLogger);
        mContext = InstrumentationRegistry.getTargetContext();
        for (CronetProvider provider : CronetProvider.getAllProviders(mContext)) {
            if (provider.getName().equals(CronetProvider.PROVIDER_NAME_APP_PACKAGED)) {
                mNativeProvider = provider;
            }
        }
        assertNotNull(mNativeProvider);
        assertTrue(NativeTestServer.startNativeTestServer(mContext));

        ContextUtils.initApplicationContext(mContext.getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        CronetTestRule.prepareTestStorage(mContext);
    }

    @After
    public void tearDown() {
        mTestLogger = null;
        CronetLoggerFactory.setLoggerForTesting(null);
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @SmallTest
    public void testCronetEngineInfoCreation() {
        CronetEngineBuilderImpl builder = new NativeCronetEngineBuilderImpl(mContext);
        CronetEngineBuilderInfo builderInfo = new CronetEngineBuilderInfo(builder);
        assertEquals(builder.publicKeyPinningBypassForLocalTrustAnchorsEnabled(),
                builderInfo.isPublicKeyPinningBypassForLocalTrustAnchorsEnabled());
        assertEquals(builder.getUserAgent(), builderInfo.getUserAgent());
        assertEquals(builder.storagePath(), builderInfo.getStoragePath());
        assertEquals(builder.quicEnabled(), builderInfo.isQuicEnabled());
        assertEquals(builder.http2Enabled(), builderInfo.isHttp2Enabled());
        assertEquals(builder.brotliEnabled(), builderInfo.isBrotliEnabled());
        assertEquals(builder.publicBuilderHttpCacheMode(), builderInfo.getHttpCacheMode());
        assertEquals(builder.experimentalOptions(), builderInfo.getExperimentalOptions());
        assertEquals(builder.networkQualityEstimatorEnabled(),
                builderInfo.isNetworkQualityEstimatorEnabled());
        assertEquals(builder.threadPriority(THREAD_PRIORITY_BACKGROUND),
                builderInfo.getThreadPriority());
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
        assertEquals(major, parsedVersion.getMajorVersion());
        assertEquals(minor, parsedVersion.getMinorVersion());
        assertEquals(build, parsedVersion.getBuildVersion());
        assertEquals(patch, parsedVersion.getPatchVersion());
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
            assertEquals(publicBuilderHttpCacheMode, cacheModeEnum.toPublicBuilderCacheMode());
        }
    }

    @Test
    @SmallTest
    public void testSetLoggerForTesting() {
        CronetLogger logger = CronetLoggerFactory.createLogger();
        assertEquals(0, mTestLogger.callsToLogCronetTrafficInfo());
        assertEquals(0, mTestLogger.callsToLogCronetEngineCreation());

        // We don't care about what's being logged.
        logger.logCronetTrafficInfo(0, null);
        assertEquals(1, mTestLogger.callsToLogCronetTrafficInfo());
        assertEquals(0, mTestLogger.callsToLogCronetEngineCreation());
        logger.logCronetEngineCreation(0, null, null, null);
        assertEquals(1, mTestLogger.callsToLogCronetTrafficInfo());
        assertEquals(1, mTestLogger.callsToLogCronetEngineCreation());
    }

    @Test
    @SmallTest
    public void testEngineCreation() throws JSONException {
        // CronetLogger is supported starting from Android Oreo.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        JSONObject staleDns = new JSONObject()
                                      .put("enable", true)
                                      .put("delay_ms", 0)
                                      .put("allow_other_network", true)
                                      .put("persist_to_disk", true)
                                      .put("persist_delay_ms", 0);
        final JSONObject jsonExperimentalOptions = new JSONObject().put("StaleDNS", staleDns);
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
                (ExperimentalCronetEngine.Builder) mNativeProvider.createBuilder();

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

        CronetEngine engine = builder.build();
        final CronetEngineBuilderInfo builderInfo = mTestLogger.getLastCronetEngineBuilderInfo();
        final CronetVersion version = mTestLogger.getLastCronetVersion();

        assertEquals(isPublicKeyPinningBypassForLocalTrustAnchorsEnabled,
                builderInfo.isPublicKeyPinningBypassForLocalTrustAnchorsEnabled());
        assertEquals(userAgent, builderInfo.getUserAgent());
        assertEquals(storagePath, builderInfo.getStoragePath());
        assertEquals(isQuicEnabled, builderInfo.isQuicEnabled());
        assertEquals(isHttp2Enabled, builderInfo.isHttp2Enabled());
        assertEquals(isBrotliEnabled, builderInfo.isBrotliEnabled());
        assertEquals(cacheMode, builderInfo.getHttpCacheMode());
        assertEquals(experimentalOptions, builderInfo.getExperimentalOptions());
        assertEquals(
                isNetworkQualityEstimatorEnabled, builderInfo.isNetworkQualityEstimatorEnabled());
        assertEquals(threadPriority, builderInfo.getThreadPriority());
        assertEquals(version.toString(), mNativeProvider.getVersion());

        assertEquals(1, mTestLogger.callsToLogCronetEngineCreation());
        assertEquals(0, mTestLogger.callsToLogCronetTrafficInfo());
    }

    @Test
    @SmallTest
    public void testEngineCreationAndTrafficInfoEngineId() {
        // CronetLogger is supported starting from Android Oreo.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        final String url = "www.example.com";
        CronetEngine.Builder engineBuilder = mNativeProvider.createBuilder();
        CronetEngine engine = engineBuilder.build();
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

        assertEquals(engineId, request1Id);
        assertEquals(engineId, request2Id);

        assertEquals(1, mTestLogger.callsToLogCronetEngineCreation());
        assertEquals(2, mTestLogger.callsToLogCronetTrafficInfo());
    }

    @Test
    @SmallTest
    public void testMultipleEngineCreationAndTrafficInfoEngineId() {
        // CronetLogger is supported starting from Android Oreo.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        final String url = "www.example.com";
        final CronetEngine.Builder engineBuilder = mNativeProvider.createBuilder();

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

        assertEquals(engine1Id, request1Id);
        assertEquals(engine2Id, request2Id);

        assertEquals(2, mTestLogger.callsToLogCronetEngineCreation());
        assertEquals(2, mTestLogger.callsToLogCronetTrafficInfo());
    }

    @Test
    @SmallTest
    public void testSuccessfulRequest() {
        // CronetLogger is supported starting from Android Oreo.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        final String url = NativeTestServer.getEchoBodyURL();
        CronetEngine.Builder engineBuilder = mNativeProvider.createBuilder();
        CronetEngine engine = engineBuilder.build();

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
        assertEquals(0, trafficInfo.getRequestHeaderSizeInBytes());
        assertNotEquals(0, trafficInfo.getRequestBodySizeInBytes());
        assertNotEquals(0, trafficInfo.getResponseHeaderSizeInBytes());
        assertNotEquals(0, trafficInfo.getResponseBodySizeInBytes());
        assertEquals(200, trafficInfo.getResponseStatusCode());
        assertNotEquals(Duration.ofSeconds(0), trafficInfo.getHeadersLatency());
        assertNotEquals(Duration.ofSeconds(0), trafficInfo.getTotalLatency());
        assertNotNull(trafficInfo.getNegotiatedProtocol());
        assertFalse(trafficInfo.wasConnectionMigrationAttempted());
        assertFalse(trafficInfo.didConnectionMigrationSucceed());

        assertEquals(1, mTestLogger.callsToLogCronetEngineCreation());
        assertEquals(1, mTestLogger.callsToLogCronetTrafficInfo());
    }

    @Test
    @SmallTest
    public void testFailedRequest() {
        // CronetLogger is supported starting from Android Oreo.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        final String url = "www.unreachable-url.com";
        CronetEngine.Builder engineBuilder = mNativeProvider.createBuilder();
        CronetEngine engine = engineBuilder.build();

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
        // When a request fails before hitting the server all these values won't be populated in the
        // actual code. Check that the logger sets them to some known defaults before logging.
        assertEquals(0, trafficInfo.getRequestHeaderSizeInBytes());
        assertEquals(0, trafficInfo.getRequestBodySizeInBytes());
        assertEquals(0, trafficInfo.getResponseHeaderSizeInBytes());
        assertEquals(0, trafficInfo.getResponseBodySizeInBytes());
        assertEquals(0, trafficInfo.getResponseStatusCode());
        assertEquals("", trafficInfo.getNegotiatedProtocol());
        assertFalse(trafficInfo.wasConnectionMigrationAttempted());
        assertFalse(trafficInfo.didConnectionMigrationSucceed());

        assertEquals(1, mTestLogger.callsToLogCronetEngineCreation());
        assertEquals(1, mTestLogger.callsToLogCronetTrafficInfo());
    }

    @Test
    @SmallTest
    public void testCanceledRequest() {
        // CronetLogger is supported starting from Android Oreo.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        final String url = NativeTestServer.getEchoBodyURL();
        CronetEngine.Builder engineBuilder = mNativeProvider.createBuilder();
        CronetEngine engine = engineBuilder.build();

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
        // When a request fails before hitting the server all these values won't be populated in the
        // actual code. Check that the logger sets them to some known defaults before logging.
        assertEquals(0, trafficInfo.getRequestHeaderSizeInBytes());
        assertEquals(0, trafficInfo.getRequestBodySizeInBytes());
        assertEquals(0, trafficInfo.getResponseHeaderSizeInBytes());
        assertEquals(0, trafficInfo.getResponseBodySizeInBytes());
        assertEquals(0, trafficInfo.getResponseStatusCode());
        assertEquals("", trafficInfo.getNegotiatedProtocol());
        assertFalse(trafficInfo.wasConnectionMigrationAttempted());
        assertFalse(trafficInfo.didConnectionMigrationSucceed());

        assertEquals(1, mTestLogger.callsToLogCronetEngineCreation());
        assertEquals(1, mTestLogger.callsToLogCronetTrafficInfo());
    }

    @Test
    @SmallTest
    public void testEmptyHeadersSize() {
        Map<String, List<String>> headers = Collections.emptyMap();
        assertEquals(0, CronetUrlRequest.estimateHeadersSizeInBytes(headers));
        headers = null;
        assertEquals(0, CronetUrlRequest.estimateHeadersSizeInBytes(headers));

        CronetUrlRequest.HeadersList headersList = new CronetUrlRequest.HeadersList();
        assertEquals(0, CronetUrlRequest.estimateHeadersSizeInBytes(headersList));
        headersList = null;
        assertEquals(0, CronetUrlRequest.estimateHeadersSizeInBytes(headersList));
    }

    @Test
    @SmallTest
    public void testNonEmptyHeadersSize() {
        Map<String, List<String>> headers = new HashMap<String, List<String>>() {
            {
                put("header1", Arrays.asList("value1", "value2")); // 7 + 6 + 6 = 19
                put("header2", null); // 19 + 7 = 26
                put("header3", Collections.emptyList()); // 26 + 7 + 0 = 33
                put(null, Arrays.asList("")); // 33 + 0 + 0 = 33
            }
        };
        assertEquals(33, CronetUrlRequest.estimateHeadersSizeInBytes(headers));

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
        assertEquals(33, CronetUrlRequest.estimateHeadersSizeInBytes(headersList));
    }
}
