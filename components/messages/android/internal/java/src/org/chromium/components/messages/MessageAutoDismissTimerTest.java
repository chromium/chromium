// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.Mockito.verify;

import android.os.Handler;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

/** Unit tests for {@link MessageAutoDismissTimer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageAutoDismissTimerTest {
    /** Ensure Runnable has been called by an active timer. */
    @Test
    @SmallTest
    public void testStartTimer() throws Exception {
        CallbackHelper callbackHelper = new CallbackHelper();
        long duration = 1;
        MessageAutoDismissTimer timer = new MessageAutoDismissTimer();
        timer.startTimer(duration, callbackHelper::notifyCalled);
        // Not flushing will make the looper blocked.
        Robolectric.flushForegroundThreadScheduler();
        callbackHelper.waitForOnly("Callback should be called by the active timer");
    }

    /** Ensure Runnable has been set as null after cancellation. */
    @Test
    @SmallTest
    public void testCancelTimer() {
        Handler h = Mockito.mock(Handler.class);
        long duration = 1;
        MessageAutoDismissTimer timer = new MessageAutoDismissTimer();
        timer.setHandlerForTesting(h);
        Runnable r = () -> {};
        timer.startTimer(duration, r);
        verify(h).postDelayed(r, duration);
        timer.cancelTimer();
        Assert.assertNull(
                "Runnable should be set as null if timer has been cancelled",
                timer.getRunnableOnTimeUpForTesting());
    }

    /**
     * Ensure #resetTimer does start timer if timer has not cancelled and does nothing if timer has
     * been cancelled.
     */
    @Test
    @SmallTest
    public void testResetTimer() {
        Handler h = Mockito.mock(Handler.class);
        long duration = 1;
        MessageAutoDismissTimer timer = new MessageAutoDismissTimer();
        timer.setHandlerForTesting(h);
        Runnable r = () -> {};
        timer.startTimer(duration, r);
        timer.resetTimer();
        Assert.assertEquals(
                "Runnable should not be null after active timer is reset",
                r,
                timer.getRunnableOnTimeUpForTesting());

        timer.cancelTimer();
        timer.resetTimer();
        Assert.assertNull(
                "Runnable should be set as null if timer has been cancelled",
                timer.getRunnableOnTimeUpForTesting());
    }
}
