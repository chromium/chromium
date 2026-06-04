// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertWithMessage;

import android.os.Build;
import android.os.ConditionVariable;

import androidx.annotation.RequiresApi;

import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.CronetLogger.CronetVersion;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/** Records the last engine creation (and traffic info) call it has received. */
public final class TestLogger extends CronetLogger {
    private final AtomicInteger mNextId = new AtomicInteger();
    private final AtomicInteger mCallsToLogCronetEngineBuilderInitializedInfo = new AtomicInteger();
    private final AtomicInteger mCallsToCronetInitializedInfo = new AtomicInteger();
    private final AtomicInteger mCallsToLogCronetEngineCreation = new AtomicInteger();
    private final AtomicInteger mCallsToLogCronetTrafficInfo = new AtomicInteger();
    private final AtomicLong mCronetEngineId = new AtomicLong();
    private final AtomicLong mCronetRequestId = new AtomicLong();
    private final AtomicReference<CronetEngineBuilderInitializedInfo>
            mCronetEngineBuilderInitializedInfo = new AtomicReference<>();
    private final AtomicReference<CronetInitializedInfo> mCronetInitializedInfo =
            new AtomicReference<>();
    private final AtomicReference<CronetTrafficInfo> mTrafficInfo = new AtomicReference<>();
    private final AtomicReference<CronetEngineBuilderInfo> mBuilderInfo = new AtomicReference<>();
    private final AtomicReference<CronetVersion> mVersion = new AtomicReference<>();
    private final AtomicReference<CronetSource> mSource = new AtomicReference<>();
    private final AtomicReference<CronetAdaptiveTrafficTerminatedInfo>
            mCronetAdaptiveTrafficTerminatedInfo = new AtomicReference<>();
    private final AtomicLong mNumberOfAvailableNetworks = new AtomicLong();
    private final AtomicBoolean mDefaultNetworkIsKnown = new AtomicBoolean();
    private final AtomicBoolean mFallbackNetworkCacheHit = new AtomicBoolean();
    private final ConditionVariable mCronetInitializedInfoCalled = new ConditionVariable();
    private final ConditionVariable mBlock = new ConditionVariable();
    private final ConditionVariable mWaitForLogCronetAdaptiveTrafficAlternateNetworkComputation =
            new ConditionVariable();
    private final ConditionVariable mWaitForLogCronetAdaptiveTrafficTerminated =
            new ConditionVariable();
    private final AtomicInteger mCallsToLogCronetUmaHistogram = new AtomicInteger();
    private final AtomicLong mLastCronetUmaBytesHash = new AtomicLong();
    private final AtomicInteger mLastCronetUmaValue = new AtomicInteger();
    private final ConditionVariable mWaitForLogCronetUmaHistogram = new ConditionVariable();

    public static final class UmaSample {
        public final long hash;
        public final int value;
        public final CronetSource source;

        public UmaSample(long hash, int value, CronetSource source) {
            this.hash = hash;
            this.value = value;
            this.source = source;
        }

        @Override
        public String toString() {
            return "UmaSample{hash=" + hash + ", value=" + value + ", source=" + source + "}";
        }
    }

    private final List<UmaSample> mUmaSamples = Collections.synchronizedList(new ArrayList<>());

    @Override
    public long generateId() {
        return mNextId.incrementAndGet();
    }

    @Override
    public void logCronetEngineBuilderInitializedInfo(CronetEngineBuilderInitializedInfo info) {
        mCallsToLogCronetEngineBuilderInitializedInfo.incrementAndGet();
        mCronetEngineBuilderInitializedInfo.set(info);
    }

    @Override
    public void logCronetInitializedInfo(CronetInitializedInfo info) {
        mCallsToCronetInitializedInfo.incrementAndGet();
        mCronetInitializedInfo.set(info);
        mCronetInitializedInfoCalled.open();
    }

    @Override
    public void logCronetEngineCreation(
            long cronetEngineId,
            CronetEngineBuilderInfo engineBuilderInfo,
            CronetVersion version,
            CronetSource source) {
        mCallsToLogCronetEngineCreation.incrementAndGet();
        mCronetEngineId.set(cronetEngineId);
        mBuilderInfo.set(engineBuilderInfo);
        mVersion.set(version);
        mSource.set(source);
    }

    @Override
    public void logCronetTrafficInfo(long cronetEngineId, CronetTrafficInfo trafficInfo) {
        mCallsToLogCronetTrafficInfo.incrementAndGet();
        mCronetRequestId.set(cronetEngineId);
        mTrafficInfo.set(trafficInfo);
        mBlock.open();
    }

    public void waitForLogCronetAdaptiveTrafficAlternateNetworkComputation() {
        assertWithMessage(
                        "TestLogger has not received any telemetry. This can happen, for example,"
                            + " if you are running tests against HttpEngine, which does not support"
                            + " TestLogger")
                .that(
                        mWaitForLogCronetAdaptiveTrafficAlternateNetworkComputation.block(
                                /* timeoutMs= */ 5000))
                .isTrue();
        mWaitForLogCronetAdaptiveTrafficAlternateNetworkComputation.close();
    }

    @Override
    public void logCronetAdaptiveTrafficAlternateNetworkComputation(
            CronetSource cronetSource,
            long numberOfAvailableNetworks,
            boolean defaultNetworkIsKnown,
            boolean fallbackNetworkCacheHit) {
        mNumberOfAvailableNetworks.set(numberOfAvailableNetworks);
        mDefaultNetworkIsKnown.set(defaultNetworkIsKnown);
        mFallbackNetworkCacheHit.set(fallbackNetworkCacheHit);
        mWaitForLogCronetAdaptiveTrafficAlternateNetworkComputation.open();
    }

    public long getNumberOfAvailableNetworks() {
        return mNumberOfAvailableNetworks.get();
    }

    public boolean getDefaultNetworkIsKnown() {
        return mDefaultNetworkIsKnown.get();
    }

    public boolean getFallbackNetworkCacheHit() {
        return mFallbackNetworkCacheHit.get();
    }

    @Override
    public void logCronetAdaptiveTrafficTerminated(CronetAdaptiveTrafficTerminatedInfo info) {
        mCronetAdaptiveTrafficTerminatedInfo.set(info);
        mWaitForLogCronetAdaptiveTrafficTerminated.open();
    }

    @Override
    public void logCronetUmaHistogram(long metricHash, int value, CronetSource source) {
        mCallsToLogCronetUmaHistogram.incrementAndGet();
        mLastCronetUmaBytesHash.set(metricHash);
        mLastCronetUmaValue.set(value);
        mUmaSamples.add(new UmaSample(metricHash, value, source));
        mWaitForLogCronetUmaHistogram.open();
    }

    public void waitForLogCronetAdaptiveTrafficTerminated() {
        assertWithMessage(
                        "TestLogger has not received any telemetry. This can happen, for example,"
                            + " if you are running tests against HttpEngine, which does not support"
                            + " TestLogger")
                .that(mWaitForLogCronetAdaptiveTrafficTerminated.block(/* timeoutMs= */ 5000))
                .isTrue();
        mWaitForLogCronetAdaptiveTrafficTerminated.close();
    }

    public CronetAdaptiveTrafficTerminatedInfo getCronetAdaptiveTrafficTerminatedInfo() {
        return mCronetAdaptiveTrafficTerminatedInfo.get();
    }

    public int callsToLogCronetEngineBuilderInitializedInfo() {
        return mCallsToLogCronetEngineBuilderInitializedInfo.get();
    }

    @RequiresApi(Build.VERSION_CODES.O)
    public int callsToLogCronetTrafficInfo() {
        return mCallsToLogCronetTrafficInfo.get();
    }

    public int callsToLogCronetEngineCreation() {
        return mCallsToLogCronetEngineCreation.get();
    }

    public void waitForCronetInitializedInfo() {
        assertWithMessage(
                        "TestLogger has not received any telemetry. This can happen, for example,"
                            + " if you are running tests against HttpEngine, which does not support"
                            + " TestLogger")
                .that(mCronetInitializedInfoCalled.block(/* timeoutMs= */ 5000))
                .isTrue();
        mCronetInitializedInfoCalled.close();
    }

    public void waitForLogCronetTrafficInfo() {
        assertWithMessage(
                        "TestLogger has not received any telemetry. This can happen, for example,"
                            + " if you are running tests against HttpEngine, which does not support"
                            + " TestLogger")
                .that(mBlock.block(/* timeoutMs= */ 5000))
                .isTrue();
        mBlock.close();
    }

    public long getLastCronetEngineId() {
        return mCronetEngineId.get();
    }

    public long getLastCronetRequestId() {
        return mCronetRequestId.get();
    }

    public CronetEngineBuilderInitializedInfo getLastCronetEngineBuilderInitializedInfo() {
        return mCronetEngineBuilderInitializedInfo.get();
    }

    public CronetInitializedInfo getLastCronetInitializedInfo() {
        return mCronetInitializedInfo.get();
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

    public void waitForLogCronetUmaHistogram() {
        assertWithMessage("TestLogger has not received any UMA telemetry.")
                .that(mWaitForLogCronetUmaHistogram.block(/* timeoutMs= */ 5000))
                .isTrue();
        mWaitForLogCronetUmaHistogram.close();
    }

    public int callsToLogCronetUmaHistogram() {
        return mCallsToLogCronetUmaHistogram.get();
    }

    public long getLastCronetUmaBytesHash() {
        return mLastCronetUmaBytesHash.get();
    }

    public int getLastCronetUmaValue() {
        return mLastCronetUmaValue.get();
    }

    public List<UmaSample> getUmaSamples() {
        synchronized (mUmaSamples) {
            return new ArrayList<>(mUmaSamples);
        }
    }

    public void clearUmaSamples() {
        mUmaSamples.clear();
    }
}
