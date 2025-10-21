// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.text.format.DateUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.WindowingMode;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;

/** Unit tests for {@link AppHeaderUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppHeaderUtilsUnitTest {
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
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
        AppHeaderUtils.setAppInDesktopWindowForTesting(false);
        AppHeaderUtils.resetHeaderCustomizationDisallowedOnExternalDisplayForOemForTesting();
        // Clear all preferences that may have been set during a test.
        mSharedPreferencesManager.getEditor().clear().commit();
    }

    @Test
    public void isActivityFocused_nullLifecycleDispatcher() {
        assertTrue(
                "Activity should be assumed to be focused if the lifecycle dispatcher is null.",
                AppHeaderUtils.isActivityFocusedAtStartup(/* lifecycleDispatcher= */ null));
    }

    @Test
    public void isActivityFocused_focusedActivityStates() {
        var activityStates =
                new int[] {
                    ActivityState.CREATED_WITH_NATIVE,
                    ActivityState.STARTED_WITH_NATIVE,
                    ActivityState.RESUMED_WITH_NATIVE
                };
        for (int state : activityStates) {
            when(mActivityLifecycleDispatcher.getCurrentActivityState()).thenReturn(state);
            assertTrue(
                    "Activity focus state is incorrect.",
                    AppHeaderUtils.isActivityFocusedAtStartup(mActivityLifecycleDispatcher));
        }
    }

    @Test
    public void isActivityFocused_unfocusedActivityStates() {
        var activityStates =
                new int[] {
                    ActivityState.PAUSED_WITH_NATIVE,
                    ActivityState.STOPPED_WITH_NATIVE,
                    ActivityState.DESTROYED
                };
        for (int state : activityStates) {
            when(mActivityLifecycleDispatcher.getCurrentActivityState()).thenReturn(state);
            assertFalse(
                    "Activity focus state is incorrect.",
                    AppHeaderUtils.isActivityFocusedAtStartup(mActivityLifecycleDispatcher));
        }
    }

    @Test
    public void isAppInDesktopWindow() {
        // Assume that the supplier is not initialized.
        assertFalse(
                "Desktop windowing mode status is incorrect.",
                AppHeaderUtils.isAppInDesktopWindow(/* desktopWindowStateManager= */ null));

        // Assume that the provider does not has a valid AppHeaderState.
        assertFalse(
                "Desktop windowing mode status is incorrect.",
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager));

        AppHeaderState state = Mockito.mock(AppHeaderState.class);
        doReturn(state).when(mDesktopWindowStateManager).getAppHeaderState();

        // Assume state not in desktop windowing mode.
        doReturn(false).when(state).isInDesktopWindow();
        assertFalse(
                "Desktop windowing mode status is incorrect.",
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager));

        // Assume state is in desktop windowing mode.
        doReturn(true).when(state).isInDesktopWindow();
        assertTrue(
                "Desktop windowing mode status is incorrect.",
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager));
    }

    @Test
    public void testrecordWindowingMode() {
        // Start in fullscreen mode.
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);
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

        // Simulate another activity resuming in fullscreen mode, count should remain 1.
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);
        assertEquals(
                "Activity count for fullscreen should now be 2.",
                2,
                mSharedPreferencesManager.readInt(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITY_COUNT.createKey(
                                WindowingMode.FULLSCREEN),
                        0));

        // Stop both activities in fullscreen mode, count should become 0.
        mFakeTimeTestRule.advanceMillis(1000);
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);
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
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);

        // Advance time.
        mFakeTimeTestRule.advanceMillis(1000);

        // Stop fullscreen mode.
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);

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
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);

        // Advance time past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS + 100);

        // Stop fullscreen mode.
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, false);

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
        AppHeaderUtils.recordWindowingMode(WindowingMode.FULLSCREEN, true);

        mFakeTimeTestRule.advanceMillis(1000);

        // Start in desktop window mode.
        AppHeaderUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, true);

        // Advance time past the cycle boundary.
        mFakeTimeTestRule.advanceMillis(CYCLE_LENGTH_MS);

        // Stop desktop window mode.
        AppHeaderUtils.recordWindowingMode(WindowingMode.DESKTOP_WINDOW, false);

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
