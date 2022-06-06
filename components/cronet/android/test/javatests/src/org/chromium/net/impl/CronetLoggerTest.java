// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static android.os.Process.THREAD_PRIORITY_BACKGROUND;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetVersion;

import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test logging functionality.
 */
@RunWith(JUnit4.class)
public final class CronetLoggerTest {
    private TestLogger mTestLogger;
    private Context mContext;

    final class TestLogger extends CronetLogger {
        private AtomicInteger mCallsToLogCronetEngineCreation = new AtomicInteger();
        private AtomicInteger mCallsToLogCronetTrafficInfo = new AtomicInteger();

        @Override
        public void logCronetEngineCreation(int cronetEngineId,
                CronetEngineBuilderInfo engineBuilderInfo, CronetVersion version,
                CronetSource source) {
            mCallsToLogCronetEngineCreation.incrementAndGet();
        }

        @Override
        public void logCronetTrafficInfo(int cronetEngineId, CronetTrafficInfo trafficInfo) {
            mCallsToLogCronetTrafficInfo.incrementAndGet();
        }

        public int callsToLogCronetTrafficInfo() {
            return mCallsToLogCronetTrafficInfo.get();
        }

        public int callsToLogCronetEngineCreation() {
            return mCallsToLogCronetEngineCreation.get();
        }
    }

    @Before
    public void setUp() {
        mTestLogger = new TestLogger();
        CronetLoggerFactory.setLoggerForTesting(mTestLogger);
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void tearDown() {
        mTestLogger = null;
        CronetLoggerFactory.setLoggerForTesting(null);
    }

    @Test
    @SmallTest
    public void testCronetEngineInfoCreation() {
        CronetEngineBuilderImpl builder = new NativeCronetEngineBuilderImpl(mContext);
        CronetEngineBuilderInfo builderInfo = new CronetEngineBuilderInfo(builder);
        assertEquals(builderInfo.isPublicKeyPinningBypassForLocalTrustAnchorsEnabled(),
                builder.publicKeyPinningBypassForLocalTrustAnchorsEnabled());
        assertEquals(builderInfo.getUserAgent(), builder.getUserAgent());
        assertEquals(builderInfo.getStoragePath(), builder.storagePath());
        assertEquals(builderInfo.isQuicEnabled(), builder.quicEnabled());
        assertEquals(builderInfo.isHttp2Enabled(), builder.http2Enabled());
        assertEquals(builderInfo.isBrotliEnabled(), builder.brotliEnabled());
        assertEquals(builderInfo.getHttpCacheMode(), builder.httpCacheMode());
        assertEquals(builderInfo.getExperimentalOptions(), builder.experimentalOptions());
        assertEquals(builderInfo.isNetworkQualityEstimatorEnabled(),
                builder.networkQualityEstimatorEnabled());
        assertEquals(builderInfo.getThreadPriority(),
                builder.threadPriority(THREAD_PRIORITY_BACKGROUND));
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
        assertEquals(parsedVersion.getMajorVersion(), major);
        assertEquals(parsedVersion.getMinorVersion(), minor);
        assertEquals(parsedVersion.getBuildVersion(), build);
        assertEquals(parsedVersion.getPatchVersion(), patch);
    }

    @Test
    @SmallTest
    public void testSetLoggerForTesting() {
        CronetLogger logger = CronetLoggerFactory.createLogger();
        assertEquals(mTestLogger.callsToLogCronetTrafficInfo(), 0);
        assertEquals(mTestLogger.callsToLogCronetEngineCreation(), 0);

        // We don't care about what's being logged.
        logger.logCronetTrafficInfo(0, null);
        assertEquals(mTestLogger.callsToLogCronetTrafficInfo(), 1);
        assertEquals(mTestLogger.callsToLogCronetEngineCreation(), 0);
        logger.logCronetEngineCreation(0, null, null, null);
        assertEquals(mTestLogger.callsToLogCronetTrafficInfo(), 1);
        assertEquals(mTestLogger.callsToLogCronetEngineCreation(), 1);
    }
}
