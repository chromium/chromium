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
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowMetricsUtils.WindowingMode;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.preferences.MultiInstanceSharedPreferences;

import java.util.Collections;

/** Unit tests for {@link MultiWindowMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiWindowMetricsUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private SharedPreferencesManager mSharedPreferencesManager;

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private static final long CYCLE_LENGTH_MS = DateUtils.DAY_IN_MILLIS;

    @Before
    public void setup() {
        mSharedPreferencesManager = MultiInstanceSharedPreferences.getInstance();
    }

    @After
    public void tearDown() {
        // Clear all preferences that may have been set during a test.
        mSharedPreferencesManager.getEditor().clear().commit();
    }

    @Test
    @DisabledTest(message = "https://crbug.com/510019356")
    public void testRecordWindowingMode() {
        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);
        assertEquals(
                "Activity count for fullscreen should be 1.",
                1,
                mSharedPreferencesManager
                        .readStringSet(
                                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(
                                        WindowingMode.FULLSCREEN),
                                Collections.emptySet())
                        .size());
        assertTrue(
                "Start time should be recorded.",
                mSharedPreferencesManager.contains(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.FULLSCREEN)));
        assertTrue(
                "Cycle start time should be recorded.",
                mSharedPreferencesManager.contains(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME));

        // Simulate another activity resuming in fullscreen mode, count should be 2.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 2, true);
        assertEquals(
                "Activity count for fullscreen should now be 2.",
                2,
                mSharedPreferencesManager
                        .readStringSet(
                                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(
                                        WindowingMode.FULLSCREEN),
                                Collections.emptySet())
                        .size());

        // Stop both activities in fullscreen mode, count should become 0.
        mFakeTimeTestRule.advanceMillis(1000);
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, false);
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 2, false);
        assertEquals(
                "Activity count for fullscreen should be 0.",
                0,
                mSharedPreferencesManager
                        .readStringSet(
                                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(
                                        WindowingMode.FULLSCREEN),
                                Collections.emptySet())
                        .size());
        assertFalse(
                "Start time should be removed.",
                mSharedPreferencesManager.contains(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.FULLSCREEN)));
        assertEquals(
                "Duration should be recorded.",
                1000,
                mSharedPreferencesManager.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN),
                        0));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/510013948")
    public void recordTimeSpentInWindowingMode_withinCycle() {
        long t0 = TimeUtils.elapsedRealtimeMillis();
        mSharedPreferencesManager.writeLong(
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, t0);

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
                mSharedPreferencesManager.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN),
                        -1L));
        assertFalse(
                "Start time for fullscreen mode should be removed.",
                mSharedPreferencesManager.contains(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.FULLSCREEN)));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/510025914")
    public void recordTimeSpentInWindowingMode_cycleBoundary_stoppingModeDurationNotLost() {
        long t0 = TimeUtils.elapsedRealtimeMillis();
        mSharedPreferencesManager.writeLong(
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, t0);

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
                mSharedPreferencesManager.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN),
                        -1L));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/510022013")
    public void recordTimeSpentInWindowingMode_cycleBoundary_activeModeHandled() {
        long t0 = TimeUtils.elapsedRealtimeMillis();
        mSharedPreferencesManager.writeLong(
                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, t0);

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
                mSharedPreferencesManager.contains(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN)));
        long newFullscreenStartTime =
                mSharedPreferencesManager.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.FULLSCREEN),
                        -1L);
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
                mSharedPreferencesManager.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.DESKTOP_WINDOW),
                        -1L));
        assertFalse(
                "Desktop window start time key should be removed.",
                mSharedPreferencesManager.contains(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.DESKTOP_WINDOW)));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/510006428")
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
                mSharedPreferencesManager.readLong(
                        MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, -1L));
        histogramWatcher.assertExpected();
    }

    @Test
    @DisabledTest(message = "https://crbug.com/510010805")
    public void testRecordWindowingMode_duplicateIds() {
        // Start in fullscreen mode with window ID 1.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);
        assertEquals(
                "Activity count for fullscreen should be 1.",
                1,
                mSharedPreferencesManager
                        .readStringSet(
                                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(
                                        WindowingMode.FULLSCREEN),
                                Collections.emptySet())
                        .size());

        // Call again with the same window ID.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, 1, true);
        assertEquals(
                "Activity count for fullscreen should still be 1.",
                1,
                mSharedPreferencesManager
                        .readStringSet(
                                MultiInstancePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(
                                        WindowingMode.FULLSCREEN),
                                Collections.emptySet())
                        .size());
    }
}
