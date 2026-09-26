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
        MultiWindowMetricsUtils.resetForTesting();
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
        long t0 = TimeUtils.currentTimeMillis();
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
        long t0 = TimeUtils.currentTimeMillis();
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
        long t0 = TimeUtils.currentTimeMillis();
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
        long t0 = TimeUtils.currentTimeMillis();

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
    public void recordTimeSpentInWindowingMode_durationClampedToCycleLength() {
        long t0 = TimeUtils.currentTimeMillis();
        MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(t0);
        // Pre-populate durationMs so that adding the cycle's active time would exceed
        // CYCLE_LENGTH_MS.
        MultiInstancePersistentStore.writeMultiWindowModeDurationMs(
                WindowingMode.FULLSCREEN, /* duration= */ 10000L);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiWindowMetricsUtils.WINDOWING_MODE_HISTOGRAM_PREFIX
                                        + MultiWindowMetricsUtils.getWindowingModeHistogramName(
                                                WindowingMode.FULLSCREEN)
                                        + MultiWindowMetricsUtils.WINDOWING_MODE_HISTOGRAM_SUFFIX,
                                (int) CYCLE_LENGTH_MS)
                        .build();

        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ true);
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS);
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ false);

        histogramWatcher.assertExpected();
    }

    @Test
    public void recordTimeSpentInWindowingMode_multipleElapsedCycles_fastForwardsEmptyCycles() {
        long t0 = TimeUtils.currentTimeMillis();

        // Complete a 10s fullscreen session in Cycle 1 ([t0, t0 + CYCLE_LENGTH_MS)).
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ true);
        mFakeTimeTestRule.advanceMillis(10000);
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ false);

        // Advance time across 3 full cycles into Cycle 4 (leaving Cycles 2 and 3 empty).
        mFakeTimeTestRule.advanceMillis(3 * CYCLE_LENGTH_MS);

        String fullscreenHistogram =
                MultiWindowMetricsUtils.WINDOWING_MODE_HISTOGRAM_PREFIX
                        + MultiWindowMetricsUtils.getWindowingModeHistogramName(
                                WindowingMode.FULLSCREEN)
                        + MultiWindowMetricsUtils.WINDOWING_MODE_HISTOGRAM_SUFFIX;
        var startWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(fullscreenHistogram, 10000).build();

        // Starting a new session in Cycle 4 should immediately emit Cycle 1's histogram and
        // fast-forward cycleStartTime to Cycle 4 without emitting phantom 24h histograms.
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ true);
        startWatcher.assertExpected();
        assertEquals(
                "Cycle start time should fast-forward to the start of Cycle 4.",
                t0 + 3 * CYCLE_LENGTH_MS,
                MultiInstancePersistentStore.readMultiWindowModeCycleStartTime());

        // Simulate an older cycleStartTime from Cycle 1 with a 20s duration while the Cycle 4
        // session is active, then stop the session after 5s.
        MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(t0);
        MultiInstancePersistentStore.writeMultiWindowModeDurationMs(
                WindowingMode.FULLSCREEN, /* duration= */ 20000L);
        var stopWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(fullscreenHistogram, 20000).build();

        mFakeTimeTestRule.advanceMillis(5000);
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ false);

        // Only Cycle 1's 20s duration should be emitted (no phantom 24h records for Cycles 2-3),
        // and Cycle 4's accumulated duration should be the 5s session.
        stopWatcher.assertExpected();
        assertEquals(
                "Cycle start time should fast-forward to Cycle 4 on stop.",
                t0 + 3 * CYCLE_LENGTH_MS,
                MultiInstancePersistentStore.readMultiWindowModeCycleStartTime());
        assertEquals(
                "Fullscreen duration in Cycle 4 should only reflect the 5s session.",
                5000,
                MultiInstancePersistentStore.readMultiWindowModeDurationMs(
                        WindowingMode.FULLSCREEN));
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

    @Test
    public void testRecordWindowingMode_unstoppedActivitiesClearedOnColdStart() {
        // Start in fullscreen mode and complete a 2000ms session so durationMs is persisted.
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ true);
        mFakeTimeTestRule.advanceMillis(2000);
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ false);

        // Resume fullscreen and desktop window modes, then simulate an unexpected power-off /
        // process death after 5000ms without receiving onStop().
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ true);
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.DESKTOP_WINDOW, /* windowId= */ 2, /* isStarted= */ true);
        mFakeTimeTestRule.advanceMillis(5000);
        MultiWindowMetricsUtils.resetForTesting();

        // Advance time by 10000ms while powered off, then cold-start and resume fullscreen for
        // 1000ms.
        mFakeTimeTestRule.advanceMillis(10000);
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ true);
        assertTrue(
                "Orphaned desktop window activities should be cleared on cold start.",
                MultiInstancePersistentStore.readMultiWindowModeActivities(
                                WindowingMode.DESKTOP_WINDOW)
                        .isEmpty());
        assertFalse(
                "Orphaned desktop window start time should be cleared on cold start.",
                MultiInstancePersistentStore.containsMultiWindowModeStartTime(
                        WindowingMode.DESKTOP_WINDOW));

        mFakeTimeTestRule.advanceMillis(1000);
        MultiWindowMetricsUtils.recordWindowingMode(
                WindowingMode.FULLSCREEN, /* windowId= */ 1, /* isStarted= */ false);

        // Total duration should only include the completed 2000ms session + the new 1000ms session
        // (excluding the unstopped 5000ms session and the 10000ms powered-off gap).
        assertEquals(
                "Duration should exclude unstopped pre-reboot session and powered-off gap.",
                3000,
                MultiInstancePersistentStore.readMultiWindowModeDurationMs(
                        WindowingMode.FULLSCREEN));
    }
}
