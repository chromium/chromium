// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.os.ConditionVariable;

import org.chromium.net.impl.CronetLogger.CronetEngineBuilderInfo;
import org.chromium.net.impl.CronetLogger.CronetSource;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.CronetLogger.CronetVersion;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/** Records the last engine creation (and traffic info) call it has received. */
public final class TestLogger extends CronetLogger {
    private AtomicInteger mNextId = new AtomicInteger();
    private final AtomicInteger mCallsToLogCronetEngineBuilderInitializedInfo = new AtomicInteger();
    private final AtomicInteger mCallsToCronetInitializedInfo = new AtomicInteger();
    private AtomicInteger mCallsToLogCronetEngineCreation = new AtomicInteger();
    private AtomicInteger mCallsToLogCronetTrafficInfo = new AtomicInteger();
    private AtomicLong mCronetEngineId = new AtomicLong();
    private AtomicLong mCronetRequestId = new AtomicLong();
    private final AtomicReference<CronetEngineBuilderInitializedInfo>
            mCronetEngineBuilderInitializedInfo = new AtomicReference<>();
    private final AtomicReference<CronetInitializedInfo> mCronetInitializedInfo =
            new AtomicReference<>();
    private AtomicReference<CronetTrafficInfo> mTrafficInfo = new AtomicReference<>();
    private AtomicReference<CronetEngineBuilderInfo> mBuilderInfo = new AtomicReference<>();
    private AtomicReference<CronetVersion> mVersion = new AtomicReference<>();
    private AtomicReference<CronetSource> mSource = new AtomicReference<>();
    private final ConditionVariable mCronetInitializedInfoCalled = new ConditionVariable();
    private final ConditionVariable mBlock = new ConditionVariable();

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

    public int callsToLogCronetEngineBuilderInitializedInfo() {
        return mCallsToLogCronetEngineBuilderInitializedInfo.get();
    }

    public int callsToLogCronetTrafficInfo() {
        return mCallsToLogCronetTrafficInfo.get();
    }

    public int callsToLogCronetEngineCreation() {
        return mCallsToLogCronetEngineCreation.get();
    }

    public void waitForCronetInitializedInfo() {
        mCronetInitializedInfoCalled.block();
        mCronetInitializedInfoCalled.close();
    }

    public void waitForLogCronetTrafficInfo() {
        mBlock.block();
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
}
