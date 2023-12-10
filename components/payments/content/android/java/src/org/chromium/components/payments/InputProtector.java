// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;

/** An InputProtector is used in payments UIs to prevent potentially unintended user interaction. */
public class InputProtector {
    // Amount of time in ms for which we ignore inputs. Note this is typically timed from when we
    // invoke the method to show the UI, so it may include time spent animating the sheet into view.
    public static final long POTENTIALLY_UNINTENDED_INPUT_THRESHOLD = 500;

    /** Clock interface so we can mock time in tests. */
    public interface Clock {
        long currentTimeMillis();
    }

    private Clock mClock;
    private long mShowTime;

    public InputProtector() {
        mClock = TimeUtils::currentTimeMillis;
    }

    /** Constructor used for tests to override the clock. */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public InputProtector(Clock clock) {
        mClock = clock;
    }

    /** Records the show time to the current time provided by the clock. */
    public void markShowTime() {
        mShowTime = mClock.currentTimeMillis();
    }

    /**
     * Returns whether an input event being processed should be ignored due to it occurring too
     * close in time to the time in which the dialog was shown.
     */
    public boolean shouldInputBeProcessed() {
        long currentTime = mClock.currentTimeMillis();
        return currentTime - mShowTime >= POTENTIALLY_UNINTENDED_INPUT_THRESHOLD;
    }
}
