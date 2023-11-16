// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import android.os.SystemClock;

/**
 * This allows us to do ratelimiting based on the time difference between the last log action and
 * the current log action. This class allows us to specify the number of samples/second we want for
 * each request.
 */
public final class RateLimiter {
    private static final long ONE_SECOND_MILLIS = 1000L;

    private final Object mLock = new Object();
    // The last tracked time
    private final int mSamplesPerSeconds;
    private int mSamplesLoggedDuringSecond;
    private long mLastPermitMillis = Long.MIN_VALUE;

    public RateLimiter(int samplesPerSeconds) {
        if (samplesPerSeconds <= 0) {
            throw new IllegalArgumentException("Expect sample rate to be > 0 sample(s) per second");
        }

        this.mSamplesPerSeconds = samplesPerSeconds;
    }

    // Check if rate limiting should happen based on a time passed or sample rate.
    public boolean tryAcquire() {
        synchronized (mLock) {
            long currentMillis = SystemClock.elapsedRealtime();

            if (mLastPermitMillis + ONE_SECOND_MILLIS <= currentMillis) {
                // reset samplesLoggedDuringSecond and stopwatch once a second has passed
                mSamplesLoggedDuringSecond = 1;
                mLastPermitMillis = currentMillis;
                return true;
            } else if (mSamplesLoggedDuringSecond < mSamplesPerSeconds) {
                mSamplesLoggedDuringSecond++;
                return true;
            }
            return false;
        }
    }
}
