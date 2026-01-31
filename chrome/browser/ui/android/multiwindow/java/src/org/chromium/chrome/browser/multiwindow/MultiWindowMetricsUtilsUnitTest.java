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
import org.mockito.MockitoAnnotations;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.multiwindow.MultiWindowMetricsUtils.WindowingMode;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link MultiWindowMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiWindowMetricsUtilsUnitTest {
    private SharedPreferencesManager mSharedPreferencesManager;

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private static final long CYCLE_LENGTH_MS = DateUtils.DAY_IN_MILLIS;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @After
    public void tearDown() {
        // Clear all preferences that may have been set during a test.
        mSharedPreferencesManager.getEditor().clear().commit();
    }

    @Test
    public void testRecordWindowingMode() {
        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);
        assertEquals(
                "Activity count for fullscreen should be 1.",
                1,
                mSharedPreferencesManager.readInt(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITY_COUNT.createKey(
                                WindowingMode.FULLSCREEN),
                        0));
        assertTrue(
                "Start time should be recorded.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.FULLSCREEN)));

        // Simulate another activity resuming in fullscreen mode, count should be 2.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);
        assertEquals(
                "Activity count for fullscreen should now be 2.",
                2,
                mSharedPreferencesManager.readInt(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITY_COUNT.createKey(
                                WindowingMode.FULLSCREEN),
                        0));

        // Stop both activities in fullscreen mode, count should become 0.
        mFakeTimeTestRule.advanceMillis(1000);
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);
        assertEquals(
                "Activity count for fullscreen should be 0.",
                0,
                mSharedPreferencesManager.readInt(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITY_COUNT.createKey(
                                WindowingMode.FULLSCREEN),
                        0));
        assertFalse(
                "Start time should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.FULLSCREEN)));
        assertEquals(
                "Duration should be recorded.",
                1000,
                mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN),
                        0));
    }

    @Test
    public void recordTimeSpentInWindowingMode_withinCycle() {
        long t0 = TimeUtils.currentTimeMillis();
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, t0);

        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);

        // Advance time.
        mFakeTimeTestRule.advanceMillis(1000);

        // Stop fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);

        // Verify duration is recorded correctly.
        assertEquals(
                "Fullscreen duration should be the time elapsed.",
                1000,
                mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN),
                        -1L));
        assertFalse(
                "Start time for fullscreen mode should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.FULLSCREEN)));
    }

    @Test
    public void recordTimeSpentInWindowingMode_cycleBoundary_stoppingModeDurationNotLost() {
        long t0 = TimeUtils.currentTimeMillis();
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, t0);

        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);

        // Advance time past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS + 100);

        // Stop fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);

        // The duration in the new cycle should be recorded.
        assertEquals(
                "Fullscreen duration in new cycle is incorrect.",
                100,
                mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN),
                        -1L));
    }

    @Test
    public void recordTimeSpentInWindowingMode_cycleBoundary_activeModeHandled() {
        long t0 = TimeUtils.currentTimeMillis();
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, t0);

        // Start in fullscreen mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);

        mFakeTimeTestRule.advanceMillis(1000);

        // Start in desktop window mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, true);

        // Advance time past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS);

        // Stop desktop window mode.
        MultiWindowMetricsUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, false);

        // Check fullscreen mode (active mode). Duration for the old cycle should have been recorded
        // and its start time updated. Its duration key is removed after recording.
        assertFalse(
                "Fullscreen duration key should be removed after histogram recording.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.FULLSCREEN)));
        long newFullscreenStartTime =
                mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
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
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(
                                WindowingMode.DESKTOP_WINDOW),
                        -1L));
        assertFalse(
                "Desktop window start time key should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(
                                WindowingMode.DESKTOP_WINDOW)));
    }
}
