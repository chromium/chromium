// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.text.format.DateUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowMetricsUtils.WindowingMode;

/** Unit tests for {@link MultiWindowMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiWindowMetricsUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private static final long CYCLE_LENGTH_MS = DateUtils.DAY_IN_MILLIS;

    @Before
    public void setup() {
        MultiInstancePersistentStore.ensureInitialized();
    }

    @After
    public void tearDown() {
        MultiInstancePersistentStore.resetForTesting();
    }

    @Test
    public void testRecordWindowingMode() {
        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);
        assertEquals(
                "Activity count for fullscreen should be 1.",
                1,
                MultiInstancePersistentStore.readMultiWindowModeActivities(WindowingMode.FULLSCREEN)
                        .size());
        assertTrue(
                "Start time should be recorded.",
                MultiInstancePersistentStore.containsMultiWindowModeStartTime(
                        WindowingMode.FULLSCREEN));
        assertTrue(
                "Cycle start time should be recorded.",
                MultiInstancePersistentStore.containsMultiWindowModeCycleStartTime());

        // Simulate another activity resuming in fullscreen mode, count should be 2.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 2, true);
        assertEquals(
                "Activity count for fullscreen should now be 2.",
                2,
                MultiInstancePersistentStore.readMultiWindowModeActivities(WindowingMode.FULLSCREEN)
                        .size());

        // Stop both activities in fullscreen mode, count should become 0.
        mFakeTimeTestRule.advanceMillis(1000);
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, false);
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 2, false);
        assertEquals(
                "Activity count for fullscreen should be 0.",
                0,
                MultiInstancePersistentStore.readMultiWindowModeActivities(WindowingMode.FULLSCREEN)
                        .size());
        assertFalse(
                "Start time should be removed.",
                MultiInstancePersistentStore.containsMultiWindowModeStartTime(
                        WindowingMode.FULLSCREEN));
        assertEquals(
                "Duration should be recorded.",
                1000,
                MultiInstancePersistentStore.readMultiWindowModeDurationMs(
                        WindowingMode.FULLSCREEN));
    }

    @Test
    public void recordTimeSpentInWindowingMode_withinCycle() {
        long t0 = TimeUtils.elapsedRealtimeMillis();
        MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(t0);

        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);

        // Advance time.
        mFakeTimeTestRule.advanceMillis(1000);

        // Stop fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, false);

        // Verify duration is recorded correctly.
        assertEquals(
                "Fullscreen duration should be the time elapsed.",
                1000,
                MultiInstancePersistentStore.readMultiWindowModeDurationMs(
                        WindowingMode.FULLSCREEN));
        assertFalse(
                "Start time for fullscreen mode should be removed.",
                MultiInstancePersistentStore.containsMultiWindowModeStartTime(
                        WindowingMode.FULLSCREEN));
    }

    @Test
    public void recordTimeSpentInWindowingMode_cycleBoundary_stoppingModeDurationNotLost() {
        long t0 = TimeUtils.elapsedRealtimeMillis();
        MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(t0);

        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);

        // Advance time past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS + 100);

        // Stop fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, false);

        // The duration in the new cycle should be recorded.
        assertEquals(
                "Fullscreen duration in new cycle is incorrect.",
                100,
                MultiInstancePersistentStore.readMultiWindowModeDurationMs(
                        WindowingMode.FULLSCREEN));
    }

    @Test
    public void recordTimeSpentInWindowingMode_cycleBoundary_activeModeHandled() {
        long t0 = TimeUtils.elapsedRealtimeMillis();
        MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(t0);

        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);

        mFakeTimeTestRule.advanceMillis(1000);

        // Start in desktop window mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, 2, true);

        // Advance time past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS);

        // Stop desktop window mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, 2, false);

        // Check fullscreen mode (active mode). Duration for the old cycle should have been recorded
        // and its start time updated. Its duration key is removed after recording.
        assertFalse(
                "Fullscreen duration key should be removed after histogram recording.",
                MultiInstancePersistentStore.containsMultiWindowModeDurationMs(
                        WindowingMode.FULLSCREEN));
        long newFullscreenStartTime =
                MultiInstancePersistentStore.readMultiWindowModeStartTime(
                        WindowingMode.FULLSCREEN, /* currentTime= */ -1L);
        // The new start time will be aligned to the cycle boundary.
        long expectedNewStartTime = t0 + CYCLE_LENGTH_MS;
        assertEquals(
                "Fullscreen start time should be updated to the new cycle start time.",
                expectedNewStartTime,
                newFullscreenStartTime);

        // Check desktop window mode (stopping mode). Its duration in the new cycle should be
        // recorded.
        assertEquals(
                "Desktop window duration in new cycle is incorrect.",
                1000,
                MultiInstancePersistentStore.readMultiWindowModeDurationMs(
                        WindowingMode.DESKTOP_WINDOW));
        assertFalse(
                "Desktop window start time key should be removed.",
                MultiInstancePersistentStore.containsMultiWindowModeStartTime(
                        WindowingMode.DESKTOP_WINDOW));
    }

    @Test
    public void recordTimeSpentInWindowingMode_cycleStartTimeUpdated() {
        long t0 = TimeUtils.elapsedRealtimeMillis();

        long expectedDuration = 30000;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiWindowMetricsUtils.WINDOWING_MODE_HISTOGRAM_PREFIX
                                        + MultiWindowMetricsUtils.getWindowingModeHistogramName(
                                                WindowingMode.FULLSCREEN)
                                        + MultiWindowMetricsUtils.WINDOWING_MODE_HISTOGRAM_SUFFIX,
                                (int) expectedDuration)
                        .build();
        // Start in fullscreen mode. Cycle start time should be initialized here.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);

        // Advance time for some duration, but not past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(expectedDuration);

        // Stop fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, false);

        // Advance time past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS);

        // Start in desktop window mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, 2, true);

        // Advance time for some duration.
        mFakeTimeTestRule.advanceMillis(1000);

        // Stop desktop window mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, 2, false);

        // Cycle start time should have been updated to the end of the first cycle.
        assertEquals(
                "Cycle start time should be updated to the end of the last recorded cycle.",
                t0 + CYCLE_LENGTH_MS,
                MultiInstancePersistentStore.readMultiWindowModeCycleStartTime());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordWindowingMode_duplicateIds() {
        // Start in fullscreen mode with window ID 1.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);
        assertEquals(
                "Activity count for fullscreen should be 1.",
                1,
                MultiInstancePersistentStore.readMultiWindowModeActivities(WindowingMode.FULLSCREEN)
                        .size());

        // Call again with the same window ID.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);
        assertEquals(
                "Activity count for fullscreen should still be 1.",
                1,
                MultiInstancePersistentStore.readMultiWindowModeActivities(WindowingMode.FULLSCREEN)
                        .size());
    }
}
