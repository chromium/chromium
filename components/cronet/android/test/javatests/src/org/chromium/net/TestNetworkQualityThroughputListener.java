// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import android.os.ConditionVariable;

import java.util.concurrent.Executor;

class TestNetworkQualityThroughputListener extends NetworkQualityThroughputListener {
    // Lock to ensure that observation counts can be updated and read by different threads.
    private final Object mLock = new Object();

    // Signals when the first throughput observation is received.
    private final ConditionVariable mWaitForThroughput = new ConditionVariable();

    private int mThroughputObservationCount;
    private Thread mExecutorThread;

    /**
     * Constructs a NetworkQualityThroughputListener that can listen to the throughput observations.
     *
     * @param executor The executor on which the observations are reported.
     */
    TestNetworkQualityThroughputListener(Executor executor) {
        super(executor);
    }

    @Override
    public void onThroughputObservation(int throughputKbps, long when, int source) {
        synchronized (mLock) {
            mWaitForThroughput.open();
            mThroughputObservationCount++;
            if (mExecutorThread == null) {
                mExecutorThread = Thread.currentThread();
            }
            // Verify that the listener is always notified on the same thread.
            assertThat(Thread.currentThread()).isEqualTo(mExecutorThread);
        }
    }

    /*
     * Blocks until the first throughput observation is received.
     */
    public void waitUntilFirstThroughputObservationReceived() {
        mWaitForThroughput.block();
    }

    public int throughputObservationCount() {
        synchronized (mLock) {
            return mThroughputObservationCount;
        }
    }

    public Thread getThread() {
        synchronized (mLock) {
            return mExecutorThread;
        }
    }
}
