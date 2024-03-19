// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Build;
import android.os.ConditionVariable;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.CronetLogger.CronetVersion;

@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public final class CronetLoggerImplTest {
    public CronetLoggerImplTest() {
        // CronetLoggerImpl only supports R+
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.R);
    }

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    private static final int CRONET_ENGINE_ID = 1;
    private static final CronetSource SOURCE = CronetSource.CRONET_SOURCE_STATICALLY_LINKED;

    private CronetLoggerImpl mCronetLoggerImpl;

    @Mock CronetEngineBuilderInfo mBuilderInfo;
    @Mock CronetVersion mVersion;
    @Mock CronetTrafficInfo mTrafficInfo;

    @Before
    public void setUp() {
        mCronetLoggerImpl = spy(new CronetLoggerImpl(1));
    }

    @Test
    public void testGenerateId() {
        long id = mCronetLoggerImpl.generateId();
        assertThat(id).isNotEqualTo(Long.MIN_VALUE);
        assertThat(id).isNotEqualTo(Long.MAX_VALUE);
        assertThat(id).isNotEqualTo(-1);
        assertThat(id).isNotEqualTo(0);
    }

    @Test
    public void testLogCronetEngineCreated_validInputs_shouldExecuteWrite() {
        mCronetLoggerImpl.logCronetEngineCreation(CRONET_ENGINE_ID, mBuilderInfo, mVersion, SOURCE);

        verify(mCronetLoggerImpl, times(1))
                .writeCronetEngineCreation(CRONET_ENGINE_ID, mBuilderInfo, mVersion, SOURCE);
    }

    @Test
    public void testLogCronetEngineCreated_invalidInputs_shouldNotExecuteWrite() {
        mCronetLoggerImpl.logCronetEngineCreation(CRONET_ENGINE_ID, null, mVersion, SOURCE);

        verify(mCronetLoggerImpl, never())
                .writeCronetEngineCreation(CRONET_ENGINE_ID, null, mVersion, SOURCE);
    }

    @Test
    public void testLogCronetTrafficInfo_validInputs_shouldExecuteWrite() {
        mCronetLoggerImpl.logCronetTrafficInfo(CRONET_ENGINE_ID, mTrafficInfo);

        verify(mCronetLoggerImpl, times(1))
                .writeCronetTrafficReported(CRONET_ENGINE_ID, mTrafficInfo, 0);
    }

    @Test
    public void testLogCronetTrafficInfo_invalidInputs_shouldNotExecuteWrite() {
        mCronetLoggerImpl.logCronetTrafficInfo(CRONET_ENGINE_ID, null);

        verify(mCronetLoggerImpl, never()).writeCronetTrafficReported(CRONET_ENGINE_ID, null, 0);
    }

    // TODO(b/309098875): the internal repo these tests were originally written in had the following
    // tests:
    // - logCronetTrafficInfo_samplesRateLimited_shouldBeZero
    // - logCronetTrafficInfo_samplesRateLimited_shouldBe2
    // - logCronetTrafficInfo_simultaneousLogs_shouldExecuteWriteOnce
    // Problem is these tests used the Roboelectric ShadowSystemClock, which we can't easily use
    // here. We should find a way to work around that. In the mean time, these tests are omitted.

    @Test
    public void testLogCronetTrafficInfo_twoSamplesPerSecond_shouldLogTwice() {
        mCronetLoggerImpl = spy(new CronetLoggerImpl(2));

        mCronetLoggerImpl.logCronetTrafficInfo(CRONET_ENGINE_ID, mTrafficInfo);
        mCronetLoggerImpl.logCronetTrafficInfo(CRONET_ENGINE_ID, mTrafficInfo);
        mCronetLoggerImpl.logCronetTrafficInfo(CRONET_ENGINE_ID, mTrafficInfo);

        verify(mCronetLoggerImpl, times(2))
                .writeCronetTrafficReported(CRONET_ENGINE_ID, mTrafficInfo, 0);
    }

    static class LogThread extends Thread {
        final CronetLoggerImpl mLogger;
        final ConditionVariable mRunBlocker;
        final CronetTrafficInfo mTrafficInfo;

        public LogThread(
                CronetLoggerImpl logger,
                ConditionVariable runBlocker,
                CronetTrafficInfo trafficInfo) {
            this.mLogger = logger;
            this.mRunBlocker = runBlocker;
            this.mTrafficInfo = trafficInfo;
        }

        @Override
        public void run() {
            mRunBlocker.block();
            mLogger.logCronetTrafficInfo(CRONET_ENGINE_ID, mTrafficInfo);
        }
    }
}
