// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTypeTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.mojom.WindowShowState;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(
        reason =
                "Tests will be flaky if batched as they create/close windows and change window"
                        + " states in quick succession")
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
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
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
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void startTwa_createsChromeAndroidTask() throws Exception {
        // Act.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");

        // Assert.
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
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
        var profile = assumeNonNull(tabModel.getProfile());

        // Assert.
        assertNotNull(chromeAndroidTask.getSessionIdForTesting(profile));
        assertNotNull(tabModel.getNativeSessionIdForTesting());
        assertEquals(
                chromeAndroidTask.getSessionIdForTesting(profile),
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
        var profile = assumeNonNull(tabModel.getProfile());

        assertNotNull(chromeAndroidTask.getSessionIdForTesting(profile));
        assertNotNull(tabModel.getNativeSessionIdForTesting());
        assertEquals(
                chromeAndroidTask.getSessionIdForTesting(profile),
                tabModel.getNativeSessionIdForTesting());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void startWebappActivity_chromeAndroidTaskAndTabModelHaveSameSessionId() {
        // Arrange.
        mWebappActivityTestRule.startWebappActivity();

        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        var tabModel = mWebappActivityTestRule.getActivity().getCurrentTabModel();
        var profile = assumeNonNull(tabModel.getProfile());

        // Assert.
        assertNotNull(chromeAndroidTask.getSessionIdForTesting(profile));
        assertNotNull(tabModel.getNativeSessionIdForTesting());
        assertEquals(
                chromeAndroidTask.getSessionIdForTesting(profile),
                tabModel.getNativeSessionIdForTesting());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void startTwa_chromeAndroidTaskAndTabModelHaveSameSessionId() throws Exception {
        // Arrange.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");

        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        var tabModel = mCustomTabActivityTestRule.getActivity().getCurrentTabModel();
        var profile = assumeNonNull(tabModel.getProfile());

        // Assert.
        assertNotNull(chromeAndroidTask.getSessionIdForTesting(profile));
        assertNotNull(tabModel.getNativeSessionIdForTesting());
        assertEquals(
                chromeAndroidTask.getSessionIdForTesting(profile),
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
        assertFalse(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isActive));

        chromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(chromeAndroidTask);
        assertTrue(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isActive));
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
                ThreadUtils.runOnUiThreadBlocking(
                                secondChromeAndroidTask::getLastActivatedTimeMillis)
                        > ThreadUtils.runOnUiThreadBlocking(
                                firstChromeAndroidTask::getLastActivatedTimeMillis));

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
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        firstChromeAndroidTask.addFeature(
                                new ChromeAndroidTaskFeatureKey(
                                        TestChromeAndroidTaskFeature.class,
                                        webPageStation.getTab().getProfile()),
                                () -> testFeature));

        // Act:
        // Open a new window. The first window will lose focus.
        // Then reactivate the first window.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);
        CriteriaHelper.pollUiThread(secondChromeAndroidTask::isActive);

        ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::activate);
        Assert.assertTrue(
                "Activate should make isActive true immediately",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        CriteriaHelper.pollUiThread(
                assumeNonNull(webPageStation.getActivity().getWindowAndroid())
                        ::isTopResumedActivity);
        Assert.assertTrue(
                "Activate should make isActive true eventually",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));

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
        Rect actualBoundsInDp = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getBoundsInDp);

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
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @DisabledTest(message = "https://crbug.com/485551955")
    public void createPendingTask_withInitialBounds_createsTaskWithCorrectBounds() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        Rect initialBoundsInDp = new Rect(100, 100, 500, 500);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        initialBoundsInDp.left,
                        initialBoundsInDp.top,
                        initialBoundsInDp.right,
                        initialBoundsInDp.bottom,
                        WindowShowState.DEFAULT);
        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> assumeNonNull(ChromeAndroidTaskTrackerFactory.getInstance()));

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () -> chromeAndroidTaskTracker.createPendingTask(createParams, null));

        // Assert.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);
        int taskId = newActivity.getTaskId();
        var chromeAndroidTask = waitForChromeAndroidTask(taskId);

        CriteriaHelper.pollUiThread(
                () -> {
                    Rect actualBoundsInDp =
                            ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getBoundsInDp);
                    Criteria.checkThat(actualBoundsInDp, Matchers.is(initialBoundsInDp));
                });

        // Cleanup.
        newActivity.finishAndRemoveTask();
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
        ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::close);

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
        assertFalse(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isActive));
        assertTrue(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));

        // Act
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::activate);

        // Assert
        Assert.assertTrue(
                "Activate should make isActive true immediately",
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isActive));
        CriteriaHelper.pollUiThread(
                assumeNonNull(webPageStation.getActivity().getWindowAndroid())
                        ::isTopResumedActivity);
        Assert.assertTrue(
                "Activate should make isActive true eventually",
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isActive));
        assertFalse(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));
        // Cleanup
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM /* test needs freeform windows */)
    public void show_taskIsVisibleButInActive_activateTask() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        var firstChromeTabbedActivity = webPageStation.getActivity();
        int firstTaskId = firstChromeTabbedActivity.getTaskId();
        var firstChromeAndroidTask = getChromeAndroidTask(firstTaskId);
        var firstWindowAndroid = firstChromeTabbedActivity.getWindowAndroid();
        assertNotNull(firstChromeAndroidTask);
        assertNotNull(firstWindowAndroid);

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);

        assertTrue(ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isVisible));
        assertFalse(ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        assertTrue(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));

        // Act
        ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::show);

        // Assert
        Assert.assertTrue(
                "Show() should make isActive() true immediately",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        CriteriaHelper.pollUiThread(firstWindowAndroid::isTopResumedActivity);
        Assert.assertTrue(
                "Show() should make isActive() true eventually",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        assertFalse(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));

        // Cleanup
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM /* test needs freeform windows */)
    public void showInactive_taskIsActive_activateAnotherTask() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        var firstChromeTabbedActivity = webPageStation.getActivity();
        int firstTaskId = firstChromeTabbedActivity.getTaskId();
        var firstChromeAndroidTask = getChromeAndroidTask(firstTaskId);
        var firstWindowAndroid = firstChromeTabbedActivity.getWindowAndroid();
        assertNotNull(firstChromeAndroidTask);
        assertNotNull(firstWindowAndroid);

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);

        assertTrue(ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isVisible));
        assertFalse(ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        assertTrue(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));

        // Act
        ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::showInactive);

        // Assert
        assertTrue(
                "2nd window's showInactive() should make 1st window's isActive() true immediately",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        CriteriaHelper.pollUiThread(firstWindowAndroid::isTopResumedActivity);
        assertTrue(
                "2nd window's showInactive() should make 1st window's isActive() true eventually",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        assertFalse(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));

        // Cleanup
        ntpStation.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM /* test needs freeform windows */)
    public void deactivate_activateVisibleInactiveTask() {
        // Arrange
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var firstChromeAndroidTask = getChromeAndroidTask(firstTaskId);

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(firstChromeAndroidTask);
        assertNotNull(secondChromeAndroidTask);
        assertFalse(ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        assertTrue(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));

        // Act
        ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::deactivate);

        // Assert
        assertTrue(
                "Deactivating the 2nd window should immediately make isActive() true for the 1st"
                        + " window",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        CriteriaHelper.pollUiThread(
                assumeNonNull(webPageStation.getActivity().getWindowAndroid())
                        ::isTopResumedActivity);
        assertTrue(
                "Deactivating the 2nd window should keep isActive() true for the 1st window after"
                        + " the 1st window becomes active",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        assertFalse(
                "Deactivating the 2nd window should keep isActive() false for the 2nd window after"
                        + " the 1st window becomes active",
                ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));

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
        assertTrue(ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));

        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);
        assertTrue(ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));
        assertFalse(
                "Task should be inactive",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));

        // Act
        ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::deactivate);

        // Assert
        assertFalse(
                "Deactivate should be a no-op for inactive tasks",
                ThreadUtils.runOnUiThreadBlocking(firstChromeAndroidTask::isActive));
        assertTrue(
                "Deactivate should be a no-op",
                ThreadUtils.runOnUiThreadBlocking(secondChromeAndroidTask::isActive));
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
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isMaximized));
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
        assertTrue(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isVisible));
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
        assertFalse(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isMinimized));
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void maximize_cannotSetBounds_noOp() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeAndroidTask =
                getChromeAndroidTask(mFreshCtaTransitTestRule.getActivity().getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act.
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::maximize);

        // Assert: the state should be IDLE as we can't change bounds.
        assertEquals(ChromeAndroidTaskImpl.State.IDLE, chromeAndroidTask.getState());
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
        assertTrue(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isVisible));
        assertFalse(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isMinimized));

        // Act
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::minimize);

        // Assert
        CriteriaHelper.pollUiThread(
                AsyncInitializationActivity::wasMoveTaskToBackInterceptedForTesting);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void restore_afterMaximize_cannotSetBounds_noOp() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeAndroidTask =
                getChromeAndroidTask(mFreshCtaTransitTestRule.getActivity().getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    chromeAndroidTask.maximize();
                    chromeAndroidTask.restore();
                });

        // Assert: the state should be IDLE as we can't change bounds.
        assertEquals(ChromeAndroidTaskImpl.State.IDLE, chromeAndroidTask.getState());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void setBoundsInDp_cannotSetBounds_noOp() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeAndroidTask =
                getChromeAndroidTask(mFreshCtaTransitTestRule.getActivity().getTaskId());
        assertNotNull(chromeAndroidTask);
        Rect currentBounds = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getBoundsInDp);

        // Act.
        Rect newBounds =
                new Rect(
                        currentBounds.left + 100,
                        currentBounds.top + 100,
                        currentBounds.right - 100,
                        currentBounds.bottom - 100);
        ThreadUtils.runOnUiThreadBlocking(() -> chromeAndroidTask.setBoundsInDp(newBounds));

        // Assert:
        // (1) The state should be IDLE as we can't change bounds.
        // (2) getBoundsInDp() should return the unchanged bounds.
        assertEquals(ChromeAndroidTaskImpl.State.IDLE, chromeAndroidTask.getState());
        assertEquals(
                currentBounds, ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getBoundsInDp));
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

    /**
     * Verifies that a {@link ChromeAndroidTask} outlives objects owned by {@code Activity}.
     *
     * <p>Many objects owned by {@link ChromeTabbedActivity} (namely {@link RootUiCoordinator})
     * destroy themselves on the activity destruction by registering to {@link
     * ActivityLifecycleDispatcher}. Since they often pass {@link ChromeAndroidTask} to JNI bridges
     * on initialization (as BrowserWindowInterface), the lifetime of {@link ChromeAndroidTask} must
     * be longer than that of those objects.
     */
    @Test
    @MediumTest
    public void destroyChromeTabbedActivity_chromeAndroidTaskOutlivesObjectsOwnedByActivity() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();

        ChromeTabbedActivity activity = mFreshCtaTransitTestRule.getActivity();
        int taskId = activity.getTaskId();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getLifecycleDispatcher()
                            .register(
                                    new DestroyObserver() {
                                        @Override
                                        public void onDestroy() {
                                            // Assert.
                                            assertNotNull(getChromeAndroidTask(taskId));
                                        }
                                    });
                });

        // Act.
        mFreshCtaTransitTestRule.finishActivity();
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
        assertFalse(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isFullscreen));
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
        assertTrue(ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isFullscreen));
    }

    @Test
    @MediumTest
    public void createBrowserWindowSync_createsDefaultChromeTabbedActivity() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL, profile, 0, 0, 0, 0, WindowShowState.DEFAULT);
        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Act.
        createBrowserWindowSync(createParams);

        // Assert.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.R)
    public void createBrowserWindowSync_createsMaximizedChromeTabbedActivity() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL, profile, 0, 0, 0, 0, WindowShowState.MAXIMIZED);
        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Act.
        createBrowserWindowSync(createParams);

        // Assert.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);

        CriteriaHelper.pollUiThread(
                () -> {
                    var windowManager = newActivity.getWindowManager();
                    Criteria.checkThat(
                            windowManager.getCurrentWindowMetrics().getBounds(),
                            Matchers.is(windowManager.getMaximumWindowMetrics().getBounds()));
                });

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.R)
    public void createBrowserWindowSync_createsMinimizedChromeTabbedActivity() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL, profile, 0, 0, 0, 0, WindowShowState.MINIMIZED);
        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Act.
        createBrowserWindowSync(createParams);

        // Assert.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);
        CriteriaHelper.pollUiThread(
                ChromeTabbedActivity::wasMoveTaskToBackInterceptedForTesting,
                "Failed to move task to the background.");

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    public void createPendingTask_requestNonOverlappingPendingActions_dispatchesBothActions() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL, profile, 0, 0, 0, 0, WindowShowState.DEFAULT);
        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var taskTracker =
                                    assumeNonNull(ChromeAndroidTaskTrackerFactory.getInstance());
                            ChromeAndroidTaskTrackerImpl
                                    .pausePendingTaskActivityCreationForTesting();
                            return taskTracker;
                        });

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Arrange : Request MAXIMIZE > DEACTIVATE on pending task.
        var chromeAndroidTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var task =
                                    chromeAndroidTaskTracker.createPendingTask(
                                            createParams, /* callback= */ null);
                            assertNotNull(task);

                            var pendingTaskInfo = task.getPendingTaskInfo();
                            assertNotNull(pendingTaskInfo);

                            task.maximize();
                            task.deactivate();

                            ChromeAndroidTaskTrackerImpl
                                    .resumePendingTaskActivityCreationForTesting(
                                            pendingTaskInfo.mPendingTaskId);

                            return task;
                        });

        // Assert: Verify that pending actions are dispatched.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);
        CriteriaHelper.pollUiThread(
                () -> {
                    var windowManager = newActivity.getWindowManager();
                    Criteria.checkThat(
                            windowManager.getCurrentWindowMetrics().getBounds(),
                            Matchers.is(windowManager.getMaximumWindowMetrics().getBounds()));
                    Criteria.checkThat(
                            assumeNonNull(newActivity.getWindowAndroid()).isTopResumedActivity(),
                            Matchers.is(false));
                },
                /* maxTimeoutMs= */ 15_000L,
                /* checkIntervalMs= */ 1000L);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(chromeAndroidTask.isMaximized());
                    assertFalse(chromeAndroidTask.isActive());
                });

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM /* test needs freeform windows */)
    public void createPendingTask_requestShowInactive_dispatchesShowInactive() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL, profile, 0, 0, 0, 0, WindowShowState.DEFAULT);
        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var taskTracker =
                                    assumeNonNull(ChromeAndroidTaskTrackerFactory.getInstance());
                            ChromeAndroidTaskTrackerImpl
                                    .pausePendingTaskActivityCreationForTesting();
                            return taskTracker;
                        });

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Arrange : Request SHOW_INACTIVE on pending task.
        var chromeAndroidTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var task =
                                    chromeAndroidTaskTracker.createPendingTask(
                                            createParams, /* callback= */ null);
                            assertNotNull(task);

                            var pendingTaskInfo = task.getPendingTaskInfo();
                            assertNotNull(pendingTaskInfo);

                            task.showInactive();

                            ChromeAndroidTaskTrackerImpl
                                    .resumePendingTaskActivityCreationForTesting(
                                            pendingTaskInfo.mPendingTaskId);

                            return task;
                        });

        // Assert: Verify that pending actions are dispatched and the task is inactive.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            assumeNonNull(newActivity.getWindowAndroid()).isTopResumedActivity(),
                            Matchers.is(false));
                },
                /* maxTimeoutMs= */ 15_000L,
                /* checkIntervalMs= */ 1000L);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(chromeAndroidTask.isActive());
                });

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    public void
            createPendingTask_requestPendingActions_lastActionHasHighestPriority_dispatchesLastAction() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL, profile, 0, 0, 0, 0, WindowShowState.DEFAULT);
        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var taskTracker =
                                    assumeNonNull(ChromeAndroidTaskTrackerFactory.getInstance());
                            ChromeAndroidTaskTrackerImpl
                                    .pausePendingTaskActivityCreationForTesting();
                            return taskTracker;
                        });
        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Arrange : Request MAXIMIZE > DEACTIVATE > MINIMIZE on pending task.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var task =
                            chromeAndroidTaskTracker.createPendingTask(
                                    createParams, /* callback= */ null);
                    assertNotNull(task);

                    var pendingTaskInfo = task.getPendingTaskInfo();
                    assertNotNull(pendingTaskInfo);

                    task.maximize();
                    task.deactivate();
                    task.minimize();

                    ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
                    ChromeAndroidTaskTrackerImpl.resumePendingTaskActivityCreationForTesting(
                            pendingTaskInfo.mPendingTaskId);
                });

        // Assert: Verify that pending MINIMIZE action is dispatched.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);
        CriteriaHelper.pollUiThread(
                ChromeTabbedActivity::wasMoveTaskToBackInterceptedForTesting,
                "Failed to move task to the background.");

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    public void
            createPendingTask_requestMultiplePendingActions_firstActionHasHighestPriority_dispatchesFirstActionOnly() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL, profile, 0, 0, 0, 0, WindowShowState.DEFAULT);
        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var taskTracker =
                                    assumeNonNull(ChromeAndroidTaskTrackerFactory.getInstance());
                            ChromeAndroidTaskTrackerImpl
                                    .pausePendingTaskActivityCreationForTesting();
                            return taskTracker;
                        });
        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Arrange : Request CLOSE > SHOW on pending task.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var task =
                            chromeAndroidTaskTracker.createPendingTask(
                                    createParams, /* callback= */ null);
                    assertNotNull(task);

                    var pendingTaskInfo = task.getPendingTaskInfo();
                    assertNotNull(pendingTaskInfo);

                    task.close();
                    task.show();

                    ChromeAndroidTaskTrackerImpl.resumePendingTaskActivityCreationForTesting(
                            pendingTaskInfo.mPendingTaskId);
                });

        // Assert: Verify that pending CLOSE action is dispatched.
        CriteriaHelper.pollUiThread(
                () -> {
                    Set<Integer> newTaskIds = getTabbedActivityTaskIds();
                    Criteria.checkThat(newTaskIds.size(), Matchers.is(currentTaskIds.size()));
                });
    }

    private static ChromeTabbedActivity waitForNewTabbedActivity(
            Set<Integer> currentTabbedActivityTaskIds) {
        AtomicReference<ChromeTabbedActivity> newActivityRef = new AtomicReference<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    for (Activity activity : ApplicationStatus.getRunningActivities()) {
                        if (activity instanceof ChromeTabbedActivity cta
                                && !currentTabbedActivityTaskIds.contains(activity.getTaskId())) {
                            newActivityRef.set(cta);
                            break;
                        }
                    }
                    return newActivityRef.get() != null;
                },
                "New ChromeTabbedActivity was not created.");

        ChromeTabbedActivity newActivity = newActivityRef.get();
        assertNotNull(newActivity);
        return newActivity;
    }

    private static Set<Integer> getTabbedActivityTaskIds() {
        Set<Integer> currentTaskIds = new HashSet<>();
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity instanceof ChromeTabbedActivity) {
                currentTaskIds.add(activity.getTaskId());
            }
        }
        return currentTaskIds;
    }

    private void createBrowserWindowSync(AndroidBrowserWindowCreateParams createParams) {
        // BrowserWindowCreatorBridge#createBrowserWindow() requires invocation on the UI thread
        // because it instantiates an AndroidBrowserWindow, whose constructor calls
        // sessions:SessionIdGenerator::NewUnique() to get a new session id.
        // SessionIdGenerator::NewUnique() uses a SequenceChecker that has a check to ensure that it
        // is always called on the same thread that it was created on.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowserWindowCreatorBridge.createBrowserWindow(createParams);
                });
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

    private @Nullable ChromeAndroidTaskImpl getChromeAndroidTask(int taskId) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var chromeAndroidTaskTracker =
                            assumeNonNull(ChromeAndroidTaskTrackerFactory.getInstance());
                    return (ChromeAndroidTaskImpl) chromeAndroidTaskTracker.get(taskId);
                });
    }

    private ChromeAndroidTaskImpl waitForChromeAndroidTask(int taskId) {
        AtomicReference<@Nullable ChromeAndroidTaskImpl> taskRef = new AtomicReference<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    taskRef.set(getChromeAndroidTask(taskId));
                    return taskRef.get() != null;
                },
                "ChromeAndroidTask was not created for taskId: " + taskId);
        var task = taskRef.get();
        assertNotNull(task);
        return task;
    }

    private static final class TestChromeAndroidTaskFeature implements ChromeAndroidTaskFeature {

        final List<Boolean> mTaskFocusChangedParams = new ArrayList<>();

        int mTimesOnTaskBoundsChanged;

        @Override
        public void onAddedToTask() {}

        @Override
        public void onFeatureRemoved() {}

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
