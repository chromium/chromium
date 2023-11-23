// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.os.Handler;

import org.chromium.base.ThreadUtils;

/** Auto dismiss timer for messages. */
class MessageAutoDismissTimer {
    private long mDuration;
    private Runnable mRunnableOnTimeUp;
    private Handler mAutoDismissTimer;

    /** @param duration Duration in mills. */
    public MessageAutoDismissTimer() {
        mAutoDismissTimer = new Handler(ThreadUtils.getUiThreadLooper());
    }

    /** Reset the timer. Do nothing if this timer has been cancelled already. */
    void resetTimer() {
        if (mRunnableOnTimeUp == null) return;
        Runnable runnable = mRunnableOnTimeUp;
        cancelTimer();
        startTimer(mDuration, runnable);
    }

    /** Cancel the timer. The registered runnable will not be run. */
    void cancelTimer() {
        if (mRunnableOnTimeUp == null) return;
        mAutoDismissTimer.removeCallbacksAndMessages(null);
        mRunnableOnTimeUp = null;
    }

    /** @param runnableOnTimeUp Runnable called when time is up. */
    void startTimer(long duration, Runnable runnableOnTimeUp) {
        mDuration = duration;
        assert mDuration > 0;
        mRunnableOnTimeUp = runnableOnTimeUp;
        mAutoDismissTimer.postDelayed(mRunnableOnTimeUp, mDuration);
    }

    void setHandlerForTesting(Handler handler) {
        mAutoDismissTimer = handler;
    }

    Runnable getRunnableOnTimeUpForTesting() {
        return mRunnableOnTimeUp;
    }
}
