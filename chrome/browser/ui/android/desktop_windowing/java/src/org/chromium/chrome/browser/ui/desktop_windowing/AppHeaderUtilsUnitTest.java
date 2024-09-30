// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;

/** Unit tests for {@link AppHeaderUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppHeaderUtilsUnitTest {
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private DesktopWindowStateProvider mDesktopWindowStateProvider;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
    }

    @Test
    public void isActivityFocused_nullLifecycleDispatcher() {
        assertTrue(
                "Activity should be assumed to be focused if the lifecycle dispatcher is null.",
                AppHeaderUtils.isActivityFocusedAtStartup(/* activityLifecycleDispatcher= */ null));
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
                AppHeaderUtils.isAppInDesktopWindow(/* appHeaderStateProvider= */ null));

        // Assume that the provider does not has a valid AppHeaderState.
        assertFalse(
                "Desktop windowing mode status is incorrect.",
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProvider));

        AppHeaderState state = Mockito.mock(AppHeaderState.class);
        doReturn(state).when(mDesktopWindowStateProvider).getAppHeaderState();

        // Assume state not in desktop windowing mode.
        doReturn(false).when(state).isInDesktopWindow();
        assertFalse(
                "Desktop windowing mode status is incorrect.",
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProvider));

        // Assume state is in desktop windowing mode.
        doReturn(true).when(state).isInDesktopWindow();
        assertTrue(
                "Desktop windowing mode status is incorrect.",
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProvider));
    }
}
