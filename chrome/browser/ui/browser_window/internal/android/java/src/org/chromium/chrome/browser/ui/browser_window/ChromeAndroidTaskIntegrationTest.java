// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayUtil;

import java.util.ArrayList;
import java.util.List;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = Batch.PER_CLASS)
@NullMarked
public class ChromeAndroidTaskIntegrationTest {

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public WebappActivityTestRule mWebappActivityTestRule = new WebappActivityTestRule();

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
    public void startCustomTabActivityAsPopup_createsChromeAndroidTask() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.POPUP);

        // Act.
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // Assert.
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);
    }

    @Test
    @MediumTest
    public void startCustomTabActivityAsNonPopup_doesNotCreateChromeAndroidTask() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.DEFAULT);

        // Act.
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // Assert.
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNull(chromeAndroidTask);
    }

    @Test
    @MediumTest
    public void startWebappActivity_createsChromeAndroidTask() {
        // Act.
        mWebappActivityTestRule.startWebappActivity();

        // Assert.
        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);
    }

    @Test
    @MediumTest
    public void startChromeTabbedActivity_chromeAndroidTaskAndTabModelHaveSameSessionId() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();

        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        var tabModel = mFreshCtaTransitTestRule.getActivity().getCurrentTabModel();

        // Assert.
        assertNotNull(chromeAndroidTask.getSessionIdForTesting());
        assertNotNull(tabModel.getNativeSessionIdForTesting());
        assertEquals(
                chromeAndroidTask.getSessionIdForTesting(),
                tabModel.getNativeSessionIdForTesting());
    }

    @Test
    @MediumTest
    public void startCustomTabActivityAsPopup_chromeAndroidTaskAndTabModelHaveSameSessionId() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.POPUP);

        // Act.
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // Assert.
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        var tabModel = mCustomTabActivityTestRule.getActivity().getCurrentTabModel();

        assertNotNull(chromeAndroidTask.getSessionIdForTesting());
        assertNotNull(tabModel.getNativeSessionIdForTesting());
        assertEquals(
                chromeAndroidTask.getSessionIdForTesting(),
                tabModel.getNativeSessionIdForTesting());
    }

    @Test
    @MediumTest
    public void startWebappActivity_chromeAndroidTaskAndTabModelHaveSameSessionId() {
        // Arrange.
        mWebappActivityTestRule.startWebappActivity();

        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        var tabModel = mWebappActivityTestRule.getActivity().getCurrentTabModel();

        // Assert.
        assertNotNull(chromeAndroidTask.getSessionIdForTesting());
        assertNotNull(tabModel.getNativeSessionIdForTesting());
        assertEquals(
                chromeAndroidTask.getSessionIdForTesting(),
                tabModel.getNativeSessionIdForTesting());
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
        var testFeature = new TestChromeAndroidTaskFeature();
        chromeAndroidTask.addFeature(testFeature);

        // Act:
        // Open a new window, which on tablet will enter split screen mode and trigger a
        // configuration change on the first window.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();

        // Assert.
        assertEquals(1, testFeature.mTimesOnTaskBoundsChanged);

        // Cleanup.
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void onTopResumedActivityChangedWithNative_invokesOnTaskFocusChangedForFeature() {
        // Arrange:
        // Launch ChromeTabbedActivity (the first window);
        // Find its ChromeAndroidTask;
        // Add a mock ChromeAndroidTaskFeature.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var firstChromeAndroidTask = getChromeAndroidTask(firstTaskId);
        assertNotNull(firstChromeAndroidTask);
        var testFeature = new TestChromeAndroidTaskFeature();
        firstChromeAndroidTask.addFeature(testFeature);

        // Act:
        // Open a new window. The first window will lose focus.
        // Then reactivate the first window.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);
        CriteriaHelper.pollUiThread(secondChromeAndroidTask::isActive);

        firstChromeAndroidTask.activate();
        CriteriaHelper.pollUiThread(firstChromeAndroidTask::isActive);

        // Assert.
        assertEquals(2, testFeature.mTaskFocusChangedParams.size());
        assertFalse(testFeature.mTaskFocusChangedParams.get(0));
        assertTrue(testFeature.mTaskFocusChangedParams.get(1));

        // Cleanup.
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @SuppressLint("NewApi" /* @MinAndroidSdkLevel already specifies the required SDK */)
    public void getBoundsInDp_returnsCorrectBounds() {
        // Arrange
        mFreshCtaTransitTestRule.startOnBlankPage();
        ChromeTabbedActivity activity = mFreshCtaTransitTestRule.getActivity();
        int taskId = activity.getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        var activityWindowAndroid = activity.getWindowAndroid();
        assertNotNull(activityWindowAndroid);

        // Act
        Rect actualBoundsInDp = chromeAndroidTask.getBoundsInDp();

        // Assert: by default, the bounds are the maximum window bounds.
        Rect expectedBoundsInPx = activity.getWindowManager().getMaximumWindowMetrics().getBounds();
        Rect expectedBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        expectedBoundsInPx,
                        1.0f / activityWindowAndroid.getDisplay().getDipScale());

        assertEquals(expectedBoundsInDp, actualBoundsInDp);
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
        CriteriaHelper.pollUiThread(
                () -> ntpStation.getActivity().isDestroyed(), "activity to be destroyed");
        assertEquals(
                "only one activity should be running",
                1,
                ApplicationStatus.getRunningActivities().size());
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

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void show_activateVisibleInactiveTask() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(firstTaskId);

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(chromeAndroidTask);
        assertNotNull(secondChromeAndroidTask);
        assertTrue(chromeAndroidTask.isVisible());
        assertFalse(chromeAndroidTask.isActive());
        assertTrue(secondChromeAndroidTask.isActive());

        // Act
        chromeAndroidTask.show();

        // Assert
        CriteriaHelper.pollUiThread(chromeAndroidTask::isActive);
        assertFalse(secondChromeAndroidTask.isActive());
        // Cleanup
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void showInactive_activateVisibleInactiveTask() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(firstTaskId);

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(chromeAndroidTask);
        assertNotNull(secondChromeAndroidTask);
        assertTrue(chromeAndroidTask.isVisible());
        assertFalse(chromeAndroidTask.isActive());
        assertTrue(secondChromeAndroidTask.isActive());

        // Act
        secondChromeAndroidTask.showInactive();

        // Assert
        CriteriaHelper.pollUiThread(chromeAndroidTask::isActive);
        assertFalse(secondChromeAndroidTask.isActive());
        // Cleanup
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void deactivate_activateVisibleInactiveTask() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(firstTaskId);

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(chromeAndroidTask);
        assertNotNull(secondChromeAndroidTask);
        assertFalse(chromeAndroidTask.isActive());
        assertTrue(secondChromeAndroidTask.isActive());

        // Act
        secondChromeAndroidTask.deactivate();

        // Assert
        CriteriaHelper.pollUiThread(chromeAndroidTask::isActive);
        assertFalse(secondChromeAndroidTask.isActive());
        // Cleanup
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* test needs "new window" in app menu */)
    public void deactivate_notTriggeredIfAlreadyInactive() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var firstChromeAndroidTask = getChromeAndroidTask(firstTaskId);
        assertNotNull(firstChromeAndroidTask);
        assertTrue(firstChromeAndroidTask.isActive());

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);
        assertTrue(secondChromeAndroidTask.isActive());
        assertFalse("Task should be inactive", firstChromeAndroidTask.isActive());

        // Act
        firstChromeAndroidTask.deactivate();

        // Assert
        assertFalse(
                "Deactivate should be a no-op for inactive tasks",
                firstChromeAndroidTask.isActive());
        assertTrue("Deactivate should be a no-op", secondChromeAndroidTask.isActive());
        // Cleanup
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    public void isMaximized_trueByDefault() {
        // Arrange
        mFreshCtaTransitTestRule.startOnBlankPage();
        Activity activity = mFreshCtaTransitTestRule.getActivity();
        int taskId = activity.getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Assert
        assertEquals(
                "only one activity should be running",
                1,
                ApplicationStatus.getRunningActivities().size());
        assertTrue(
                "App should be maximized in non desktop windowing mode",
                chromeAndroidTask.isMaximized());
    }

    @Test
    @MediumTest
    public void isVisible_trueByDefault() {
        // Arrange
        mFreshCtaTransitTestRule.startOnBlankPage();
        Activity activity = mFreshCtaTransitTestRule.getActivity();
        int taskId = activity.getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Assert Initial states
        assertTrue(chromeAndroidTask.isVisible());
    }

    @Test
    @MediumTest
    public void isMinimized_falseByDefault() {
        // Arrange
        mFreshCtaTransitTestRule.startOnBlankPage();
        Activity activity = mFreshCtaTransitTestRule.getActivity();
        int taskId = activity.getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Assert Initial states
        assertFalse(chromeAndroidTask.isMinimized());
    }

    @Test
    @MediumTest
    public void minimize_moveTaskToBack() {
        // Arrange
        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        mFreshCtaTransitTestRule.startOnBlankPage();
        Activity activity = mFreshCtaTransitTestRule.getActivity();
        int taskId = activity.getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Assert Initial states
        assertTrue(chromeAndroidTask.isVisible());
        assertFalse(chromeAndroidTask.isMinimized());

        // Act
        chromeAndroidTask.minimize();

        // Assert
        CriteriaHelper.pollUiThread(
                AsyncInitializationActivity::wasMoveTaskToBackInterceptedForTesting);
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

    @Test
    @MediumTest
    public void isFullscreen_falseByDefault() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Assert.
        assertFalse(chromeAndroidTask.isFullscreen());
    }

    @Test
    @MediumTest
    public void isFullscreen_trueWhenFullscreen() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                /* tab= */ mFreshCtaTransitTestRule.getActivityTab(),
                /* state= */ true,
                /* activity= */ mFreshCtaTransitTestRule.getActivity());

        // Assert.
        assertTrue(chromeAndroidTask.isFullscreen());
    }

    private Intent createCustomTabIntent(@CustomTabsUiType int customTabsUiType) {
        var intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mCustomTabActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/about.html"));
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, customTabsUiType);
        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
    }

    private @Nullable ChromeAndroidTask getChromeAndroidTask(int taskId) {
        var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
        assertNotNull(chromeAndroidTaskTracker);

        return chromeAndroidTaskTracker.get(taskId);
    }

    private static final class TestChromeAndroidTaskFeature implements ChromeAndroidTaskFeature {

        final List<Boolean> mTaskFocusChangedParams = new ArrayList<>();

        int mTimesOnTaskBoundsChanged;

        @Override
        public void onAddedToTask() {}

        @Override
        public void onTaskRemoved() {}

        @Override
        public void onTaskBoundsChanged(Rect newBoundsInDp) {
            mTimesOnTaskBoundsChanged++;
        }

        @Override
        public void onTaskFocusChanged(boolean hasFocus) {
            mTaskFocusChangedParams.add(hasFocus);
        }
    }
}
