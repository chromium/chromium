// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.cma.backend.android;

/* Implements a simple configurable log throttler. */
class ThrottledLog {
    public interface LogFunction { void accept(String tag, String msg); }

    private final LogFunction mLogFct;

    private final int mInitialUnthrottledLogs; // initial logs allowed before throttling starts
    private final long mThrottlePeriodNsec; // allow one log per throttle period
    private final long mResetTimeNsec; // reset after this time

    private long mTotalLogCalls;
    private long mLastTimeLoggedNsec;
    private long mUnthrottledCount;

    ThrottledLog(LogFunction logFct, int initialUnthrottledLogs, long throttlePeriodMsec,
            long resetTimeMsec) {
        mLogFct = logFct;
        mTotalLogCalls = 0;
        mInitialUnthrottledLogs = initialUnthrottledLogs;
        mThrottlePeriodNsec = throttlePeriodMsec * 1000000;
        mResetTimeNsec = resetTimeMsec * 1000000;
        reset(System.nanoTime());
    }

    private void reset(long now) {
        mUnthrottledCount = 0;
        mLastTimeLoggedNsec = now;
    }

    public void log(String tag, String text) {
        mTotalLogCalls++;
        long now = System.nanoTime();
        long timeSinceLastLogNsec = now - mLastTimeLoggedNsec;

        if (timeSinceLastLogNsec > mResetTimeNsec) {
            reset(now);
        }

        boolean logIt = false;
        if (mUnthrottledCount < mInitialUnthrottledLogs) {
            logIt = true;
            mUnthrottledCount++;
        } else if (timeSinceLastLogNsec > mThrottlePeriodNsec) {
            logIt = true;
        }
        if (logIt) {
            mLogFct.accept(tag, "[" + mTotalLogCalls + "] " + text);
            mLastTimeLoggedNsec = now;
        }
    }
}
