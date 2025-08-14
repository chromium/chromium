// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = Batch.PER_CLASS)
@NullMarked
public class ChromeAndroidTaskIntegrationTest {

    @Rule
    public FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @MediumTest
    public void startChromeTabbedActivity_createsChromeAndroidTask() {
        // Arrange & Act.
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();

        // Assert.
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void startChromeTabbedActivity_activeChromeAndroidTask_isActive() {
        // Arrange & Act.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();

        // Assert.
        var chromeAndroidTask = getChromeAndroidTask(firstTaskId);
        assertNotNull(chromeAndroidTask);
        assertFalse(chromeAndroidTask.isActive());

        chromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(chromeAndroidTask);
        assertTrue(chromeAndroidTask.isActive());
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void getLastActivatedTimeMillis_returnsCorrectTimestampForEachTask() {
        // Arrange & Act.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();

        // Assert.
        var firstChromeAndroidTask = getChromeAndroidTask(firstTaskId);
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(firstChromeAndroidTask);
        assertNotNull(secondChromeAndroidTask);
        assertTrue(
                secondChromeAndroidTask.getLastActivatedTimeMillis()
                        > firstChromeAndroidTask.getLastActivatedTimeMillis());

        // Cleanup.
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @Restriction(
            // Test needs "new window" in app menu and the tablet behavior to enter split screen
            // mode to trigger onConfigurationChanged().
            DeviceFormFactor.ONLY_TABLET)
    public void onConfigurationChanged_invokesOnTaskBoundsChangedForFeature() {
        // Arrange:
        // Launch ChromeTabbedActivity;
        // Find its ChromeAndroidTask;
        // Add a mock ChromeAndroidTaskFeature.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);
        var mockFeature = mock(ChromeAndroidTaskFeature.class);
        chromeAndroidTask.addFeature(mockFeature);

        // Act:
        // Open a new window, which on tablet will enter split screen mode and trigger a
        // configuration change on the first window.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();

        // Assert.
        verify(mockFeature, times(1)).onTaskBoundsChanged(any());

        // Cleanup.
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    public void getBounds_returnsCorrectBounds() {
        // Arrange
        mFreshCtaTransitTestRule.startOnBlankPage();
        Activity activity = mFreshCtaTransitTestRule.getActivity();
        int taskId = activity.getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act
        Rect actualBounds = chromeAndroidTask.getBounds();

        // Assert: by default, the bounds are the maximum window bounds.
        var expectedBounds = activity.getWindowManager().getMaximumWindowMetrics().getBounds();
        assertFalse(actualBounds.isEmpty());
        assertEquals(expectedBounds, actualBounds);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void close_finishTask() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);

        // Act
        secondChromeAndroidTask.close();

        // Assert
        assertTrue(ntpStation.getActivity().isFinishing());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void activate_moveToFront() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(firstTaskId);
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(chromeAndroidTask);
        assertNotNull(secondChromeAndroidTask);
        assertFalse(chromeAndroidTask.isActive());
        assertTrue(secondChromeAndroidTask.isActive());

        // Act
        chromeAndroidTask.activate();

        // Assert
        CriteriaHelper.pollUiThread(chromeAndroidTask::isActive);
        assertFalse(secondChromeAndroidTask.isActive());
        // Cleanup
        ntpStation.getActivity().finish();
    }

    /**
     * Verifies that a {@link ChromeAndroidTask} is destroyed with its {@code Activity}.
     *
     * <p>This is the right behavior when {@link ChromeAndroidTask} tracks an {@code Activity},
     * which is a workaround to track a Task (window).
     *
     * <p>If {@link ChromeAndroidTask} tracks a Task, it should continue to exist as long as the
     * Task is alive.
     *
     * <p>Please see the documentation of {@link ChromeAndroidTask} for details.
     */
    @Test
    @MediumTest
    public void destroyChromeTabbedActivity_destroysChromeAndroidTask() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();

        // Act.
        mFreshCtaTransitTestRule.finishActivity();

        // Assert.
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNull(chromeAndroidTask);
    }

    private @Nullable ChromeAndroidTask getChromeAndroidTask(int taskId) {
        var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
        assertNotNull(chromeAndroidTaskTracker);

        return chromeAndroidTaskTracker.get(taskId);
    }
}
