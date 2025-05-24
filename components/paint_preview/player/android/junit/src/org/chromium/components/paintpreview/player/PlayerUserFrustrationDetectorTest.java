// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

/** Tests for the {@link PlayerUserFrustrationDetector} class */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerUserFrustrationDetectorTest {
    @Test
    public void testConsecutiveTapDetection() {
        CallbackHelper detectionCallback = new CallbackHelper();
        PlayerUserFrustrationDetector detector =
                new PlayerUserFrustrationDetector(detectionCallback::notifyCalled);

        final int tapsWindow = PlayerUserFrustrationDetector.CONSECUTIVE_SINGLE_TAP_WINDOW_MS;
        final int tapsCount = PlayerUserFrustrationDetector.CONSECUTIVE_SINGLE_TAP_COUNT;
        long startTime = System.currentTimeMillis();

        // Record |tapsCount| consecutive taps in shorter than |tapsWindow| intervals. This should
        // trigger the detection.
        Assert.assertEquals(
                "Frustration callback shouldn't have been called",
                0,
                detectionCallback.getCallCount());
        for (int i = 0; i < tapsCount; i++) {
            detector.recordUnconsumedTap(startTime + i * ((long) tapsWindow));
        }
        Assert.assertEquals(
                "Frustration callback should have been called once",
                1,
                detectionCallback.getCallCount());

        // A new tap, even if it's within the window, should not trigger the callback.
        detector.recordUnconsumedTap(startTime + tapsCount * tapsWindow);
        Assert.assertEquals(
                "Frustration callback shouldn't have been called",
                1,
                detectionCallback.getCallCount());

        // Perform |tapsCount - 1| series of consecutive taps. Each time, delay one of the taps
        // so it's out of the |tapsWindow|.
        // None of these should result in a frustration trigger.
        for (int i = 1; i < tapsCount; i++) {
            // Increment start time so it won't be within a valid window with the previous tap.
            startTime += (tapsCount + 2) * tapsWindow;
            for (int j = 0; j < tapsCount; j++) {
                int delay = i == j ? 10 : 0;
                detector.recordUnconsumedTap(startTime + j * ((long) tapsWindow) + delay);
            }
            Assert.assertEquals(
                    "Frustration callback shouldn't have been called",
                    1,
                    detectionCallback.getCallCount());
        }

        // Perform a valid tap sequence.
        startTime += (tapsCount + 2) * tapsWindow;
        for (int i = 0; i < tapsCount; i++) {
            detector.recordUnconsumedTap(startTime + i * ((long) tapsWindow));
        }
        Assert.assertEquals(
                "Frustration callback should have been called once",
                2,
                detectionCallback.getCallCount());
    }
}
