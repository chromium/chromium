// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.TaskLocation;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.OutcomeReceiver;

import androidx.annotation.RequiresApi;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowResizePrecheckResult;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.mojom.WindowShowState;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DisableFeatures({
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL,
    // TODO(b/555414915): Update Android tests with WebUI NTP enabled on AL.
    ChromeFeatureList.USE_WEB_UI_NTP_ANDROID
})
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

    /**
     * The acceptable margin of error (in dp) when comparing expected window bounds and actual
     * window bounds. This is to counter the rounding error during dp->px conversion.
     */
    private static final int BOUNDS_CHECK_TOLERANCE_DP = 2;

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
    public void startCustomTabActivity_createsChromeAndroidTask() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.DEFAULT);

        // Act.
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // Assert.
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);
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
    public void startCustomTabActivity_chromeAndroidTaskAndTabModelHaveSameSessionId() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.DEFAULT);

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
    public void
            getValidProfilesForActivity_singleProfileMode_initialProfileIsRegular_returnsOnlyRegularProfile() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ChromeTabbedActivity activity = mFreshCtaTransitTestRule.getActivity();
        var activityWindowAndroid = activity.getWindowAndroid();
        assertNotNull(activityWindowAndroid);
        var chromeAndroidTask = getChromeAndroidTask(activity.getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act.
        List<Profile> profiles =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTask.getValidProfilesForActivity(activityWindowAndroid));

        // Assert.
        assertEquals(1, profiles.size());
        assertFalse(profiles.get(0).isOffTheRecord());
    }

    @Test
    @MediumTest
    public void
            getValidProfilesForActivity_singleProfileMode_initialProfileIsIncognito_returnsOnlyIncognitoProfile() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnIncognitoBlankPage();
        ChromeTabbedActivity activity = mFreshCtaTransitTestRule.getActivity();
        var activityWindowAndroid = activity.getWindowAndroid();
        assertNotNull(activityWindowAndroid);
        var chromeAndroidTask = getChromeAndroidTask(activity.getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act.
        List<Profile> profiles =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTask.getValidProfilesForActivity(activityWindowAndroid));

        // Assert.
        assertEquals(1, profiles.size());
        assertTrue(profiles.get(0).isOffTheRecord());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void
            getValidProfilesForActivity_mixedProfileMode_initialProfileIsRegular_returnsOnlyRegularProfile() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ChromeTabbedActivity activity = mFreshCtaTransitTestRule.getActivity();
        var activityWindowAndroid = activity.getWindowAndroid();
        assertNotNull(activityWindowAndroid);
        var chromeAndroidTask = getChromeAndroidTask(activity.getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act.
        List<Profile> profiles =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTask.getValidProfilesForActivity(activityWindowAndroid));

        // Assert.
        assertEquals(1, profiles.size());
        assertFalse(profiles.get(0).isOffTheRecord());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void
            getValidProfilesForActivity_mixedProfileMode_initialProfileIsRegular_switchToIncognito_returnsBothRegularAndIncognitoProfiles() {
        // Arrange.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        ChromeTabbedActivity activity = mFreshCtaTransitTestRule.getActivity();
        var activityWindowAndroid = activity.getWindowAndroid();
        assertNotNull(activityWindowAndroid);
        var chromeAndroidTask = getChromeAndroidTask(activity.getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act: Open an incognito tab in the same Activity.
        webPageStation.openNewIncognitoTabFast();
        List<Profile> profiles =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTask.getValidProfilesForActivity(activityWindowAndroid));

        // Assert.
        assertEquals(2, profiles.size());
        assertTrue(profiles.stream().anyMatch(profile -> !profile.isOffTheRecord()));
        assertTrue(profiles.stream().anyMatch(Profile::isOffTheRecord));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void
            getValidProfilesForActivity_mixedProfileMode_initialProfileIsIncognito_returnsBothRegularAndIncognitoProfiles() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnIncognitoBlankPage();
        ChromeTabbedActivity activity = mFreshCtaTransitTestRule.getActivity();
        var activityWindowAndroid = activity.getWindowAndroid();
        assertNotNull(activityWindowAndroid);
        var chromeAndroidTask = getChromeAndroidTask(activity.getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act.
        List<Profile> profiles =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTask.getValidProfilesForActivity(activityWindowAndroid));

        // Assert.
        assertEquals(2, profiles.size());
        assertTrue(profiles.stream().anyMatch(profile -> !profile.isOffTheRecord()));
        assertTrue(profiles.stream().anyMatch(Profile::isOffTheRecord));
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
    @RequiresApi(Build.VERSION_CODES.R)
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET /* non-desktop windowing mode */)
    public void getBoundsInDp_nonDesktopWindowingMode_returnsMaximizedBounds() {
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
        assertBoundsCloseEnoughInDp(expectedBoundsInDp, actualBoundsInDp);
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
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET /* non-desktop windowing mode */)
    public void isMaximized_nonDesktopWindowingMode_trueByDefault() {
        // Arrange
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Assert
        assertTrue(
                "In non-desktop windowing mode, Task should be maximized by default",
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isMaximized));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM /* desktop windowing mode */)
    public void isMaximized_desktopWindowingMode_falseByDefault() {
        // Arrange
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Assert
        assertFalse(
                "In desktop windowing mode, Task shouldn't be maximized by default",
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
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void canResize_customTabActivity_returnsFailure() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.DEFAULT);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act.
        @WindowResizePrecheckResult
        int resizePrecheckResult = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::canResize);

        // Assert: It should return a failure code on a non-freeform form factor.
        assertNotEquals(WindowResizePrecheckResult.OK, resizePrecheckResult);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void canResize_webappActivity_returnsFailure() {
        // Arrange.
        mWebappActivityTestRule.startWebappActivity();
        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act.
        @WindowResizePrecheckResult
        int resizePrecheckResult = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::canResize);

        // Assert: It should return a failure code on a non-freeform form factor.
        assertNotEquals(WindowResizePrecheckResult.OK, resizePrecheckResult);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void canResize_twaActivity_returnsFailure() throws Exception {
        // Arrange.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act.
        @WindowResizePrecheckResult
        int resizePrecheckResult = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::canResize);

        // Assert: It should return a failure code on a non-freeform form factor.
        assertNotEquals(WindowResizePrecheckResult.OK, resizePrecheckResult);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void canResize_customTabActivity_returnsOk() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.DEFAULT);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act.
        @WindowResizePrecheckResult
        int resizePrecheckResult = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::canResize);

        // Assert: It should return OK on a freeform form factor.
        // Because Chrome created this CCT, this is not NULL_APP_TASK.
        assertEquals(WindowResizePrecheckResult.OK, resizePrecheckResult);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void canResize_webappActivity_returnsOk() {
        // Arrange.
        mWebappActivityTestRule.startWebappActivity();
        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act.
        @WindowResizePrecheckResult
        int resizePrecheckResult = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::canResize);

        // Assert: It should return OK on a freeform form factor.
        assertEquals(WindowResizePrecheckResult.OK, resizePrecheckResult);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void canResize_twaActivity_returnsOk() throws Exception {
        // Arrange.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        // Act.
        @WindowResizePrecheckResult
        int resizePrecheckResult = ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::canResize);

        // Assert: It should return OK on a freeform form factor.
        assertEquals(WindowResizePrecheckResult.OK, resizePrecheckResult);
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
        // The production code relies on WindowInsetsAnimationListener#onEnd to update the window
        // state to full screen, so we need to wait for the animation here. Otherwise, the test will
        // be flaky.
        CriteriaHelper.pollUiThread(
                chromeAndroidTask::isFullscreen,
                /* maxTimeoutMs= */ 5000L,
                /* checkIntervalMs= */ 1000L);
    }

    @Test
    @MediumTest
    public void createBrowserWindowSync_initialShowStateIsDefault_createsNewTask() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        /* leftBound= */ 0,
                        /* topBound= */ 0,
                        /* rightBound= */ 0,
                        /* bottomBound= */ 0,
                        /* initialShowState= */ WindowShowState.DEFAULT,
                        /* webContents= */ null);
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
    public void createBrowserWindowSync_initialShowStateIsMinimized_minimizesNewTask() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        /* leftBound= */ 0,
                        /* topBound= */ 0,
                        /* rightBound= */ 0,
                        /* bottomBound= */ 0,
                        /* initialShowState= */ WindowShowState.MINIMIZED,
                        /* webContents= */ null);
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
    public void createPendingTask_clearsPendingIdExtraAfterActivityLaunch() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        /* leftBound= */ 0,
                        /* topBound= */ 0,
                        /* rightBound= */ 0,
                        /* bottomBound= */ 0,
                        /* initialShowState= */ WindowShowState.DEFAULT,
                        /* webContents= */ null);

        // Act.
        ChromeAndroidTaskImpl newTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var chromeAndroidTaskTracker =
                                    ChromeAndroidTaskTrackerImpl.getInstance();
                            return chromeAndroidTaskTracker.createPendingTask(
                                    createParams, /* callback= */ null);
                        });
        CriteriaHelper.pollUiThread(
                () -> newTask.getState() == ChromeAndroidTaskImpl.State.IDLE,
                /* maxTimeoutMs= */ 10_000L,
                /* checkIntervalMs= */ 1000L);

        // Assert.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activityWindowAndroid = newTask.getTopActivityWindowAndroid();
                    assertNotNull(activityWindowAndroid);

                    var activity = activityWindowAndroid.getActivity().get();
                    assertNotNull(activity);

                    assertFalse(
                            IntentUtils.safeHasExtra(
                                    activity.getIntent(),
                                    ChromeAndroidTaskTracker.EXTRA_PENDING_BROWSER_WINDOW_TASK_ID));
                });

        // Cleanup:
        ThreadUtils.runOnUiThreadBlocking(newTask::close);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM /* test needs freeform windows */)
    public void createPendingTask_requestShowInactive_deactivateNewTask() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        var profile = mFreshCtaTransitTestRule.getProfile(/* incognito= */ false);
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        ChromeAndroidTask existingTask = getChromeAndroidTask(taskId);
        assertNotNull(existingTask);

        ChromeAndroidTaskTrackerImpl taskTracker =
                ThreadUtils.runOnUiThreadBlocking(ChromeAndroidTaskTrackerImpl::getInstance);
        taskTracker.pausePendingTaskActivityCreationForTesting();

        // Act : Request SHOW_INACTIVE on pending task.
        ChromeAndroidTaskImpl newTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AndroidBrowserWindowCreateParams createParams =
                                    AndroidBrowserWindowCreateParamsImpl.create(
                                            BrowserWindowType.NORMAL,
                                            profile,
                                            /* leftBound= */ 0,
                                            /* topBound= */ 0,
                                            /* rightBound= */ 0,
                                            /* bottomBound= */ 0,
                                            /* initialShowState= */ WindowShowState.DEFAULT,
                                            /* webContents= */ null);
                            var pendingTask =
                                    taskTracker.createPendingTask(
                                            createParams, /* callback= */ null);
                            assertNotNull(pendingTask);

                            var pendingTaskInfo = pendingTask.getPendingTaskInfo();
                            assertNotNull(pendingTaskInfo);

                            pendingTask.showInactive();

                            taskTracker.resumePendingTaskActivityCreationForTesting(
                                    pendingTaskInfo.mPendingTaskId);
                            return pendingTask;
                        });

        // Assert:
        CriteriaHelper.pollUiThread(
                () ->
                        newTask.getState() != ChromeAndroidTaskImpl.State.PENDING_CREATE
                                && !newTask.isActive()
                                && existingTask.isActive(),
                /* maxTimeoutMs= */ 10_000L,
                /* checkIntervalMs= */ 1000L);

        // Cleanup.
        ThreadUtils.runOnUiThreadBlocking(newTask::close);
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
                        BrowserWindowType.NORMAL,
                        profile,
                        0,
                        0,
                        0,
                        0,
                        WindowShowState.DEFAULT,
                        null);
        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var taskTracker = ChromeAndroidTaskTrackerImpl.getInstance();
                            taskTracker.pausePendingTaskActivityCreationForTesting();
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
                    chromeAndroidTaskTracker.resumePendingTaskActivityCreationForTesting(
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
                        BrowserWindowType.NORMAL,
                        profile,
                        0,
                        0,
                        0,
                        0,
                        WindowShowState.DEFAULT,
                        null);
        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var taskTracker = ChromeAndroidTaskTrackerImpl.getInstance();
                            taskTracker.pausePendingTaskActivityCreationForTesting();
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

                    chromeAndroidTaskTracker.resumePendingTaskActivityCreationForTesting(
                            pendingTaskInfo.mPendingTaskId);
                });

        // Assert: Verify that pending CLOSE action is dispatched.
        CriteriaHelper.pollUiThread(
                () -> {
                    Set<Integer> newTaskIds = getTabbedActivityTaskIds();
                    Criteria.checkThat(newTaskIds.size(), Matchers.is(currentTaskIds.size()));
                },
                /* maxTimeoutMs= */ 10_000L,
                /* checkIntervalMs= */ 1000L);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void taskBoundsChangedByResizingWindow_invokesOnTaskBoundsChangedForFeature()
            throws Exception {
        assumeBrowserRole();
        testOnTaskBoundsChangedForFeature(/* deltaBounds= */ new Rect(100, 100, -100, -100));
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void taskBoundsChangedByMovingWindow_invokesOnTaskBoundsChangedForFeature()
            throws Exception {
        assumeBrowserRole();
        testOnTaskBoundsChangedForFeature(/* deltaBounds= */ new Rect(30, 30, 30, 30));
    }

    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    private void testOnTaskBoundsChangedForFeature(Rect deltaBounds) throws Exception {
        // Arrange:
        // Launch ChromeTabbedActivity;
        // Find its ChromeAndroidTask;
        // Add a mock ChromeAndroidTaskFeature.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeTabbedActivity = webPageStation.getActivity();
        var chromeAndroidTask = getChromeAndroidTask(chromeTabbedActivity.getTaskId());
        assertNotNull(chromeAndroidTask);
        var testFeature = new TestChromeAndroidTaskFeature();
        var featureKey =
                new ChromeAndroidTaskFeatureKey(
                        TestChromeAndroidTaskFeature.class, webPageStation.getTab().getProfile());
        ThreadUtils.runOnUiThreadBlocking(
                () -> chromeAndroidTask.addFeature(featureKey, () -> testFeature));

        // Act:
        Rect currentBounds =
                chromeTabbedActivity.getWindowManager().getCurrentWindowMetrics().getBounds();
        Rect expectedNewBoundsInPx =
                new Rect(
                        currentBounds.left + deltaBounds.left,
                        currentBounds.top + deltaBounds.top,
                        currentBounds.right + deltaBounds.right,
                        currentBounds.bottom + deltaBounds.bottom);
        Rect expectedNewBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        expectedNewBoundsInPx, 1.0f / getDipScale(chromeTabbedActivity));
        setBounds(chromeTabbedActivity, expectedNewBoundsInPx);

        // Assert:
        CriteriaHelper.pollUiThread(
                () -> {
                    List<Rect> boundsChangePxHistory = testFeature.mTaskBoundsChangePxHistory;
                    List<Rect> boundsChangeDpHistory = testFeature.mTaskBoundsChangeDpHistory;
                    if (boundsChangePxHistory.isEmpty() || boundsChangeDpHistory.isEmpty()) {
                        return false;
                    }

                    assertEquals(1, boundsChangePxHistory.size());
                    assertEquals(1, boundsChangeDpHistory.size());

                    Rect actualNewBoundsInPx = boundsChangePxHistory.get(0);
                    Rect actualNewBoundsInDp = boundsChangeDpHistory.get(0);
                    return areBoundsCloseEnough(expectedNewBoundsInDp, actualNewBoundsInDp)
                            && areBoundsCloseEnough(expectedNewBoundsInPx, actualNewBoundsInPx);
                },
                /* maxTimeoutMs= */ 5000L,
                /* checkIntervalMs= */ 1000L);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void getBoundsInDp_afterWindowIsResized_returnsCorrectBounds() throws Exception {
        assumeBrowserRole();
        testGetBoundsInDpAfterWindowBoundsChange(/* deltaBounds= */ new Rect(100, 100, -100, -100));
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void getBoundsInDp_afterWindowIsMoved_returnsCorrectBounds() throws Exception {
        assumeBrowserRole();
        testGetBoundsInDpAfterWindowBoundsChange(/* deltaBounds= */ new Rect(50, 50, 50, 50));
    }

    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    private void testGetBoundsInDpAfterWindowBoundsChange(Rect deltaBounds) throws Exception {
        // Arrange:
        // Launch ChromeTabbedActivity;
        // Find its ChromeAndroidTask;
        // Add a mock ChromeAndroidTaskFeature.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeTabbedActivity = webPageStation.getActivity();
        var chromeAndroidTask = getChromeAndroidTask(chromeTabbedActivity.getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act:
        Rect currentBounds =
                chromeTabbedActivity.getWindowManager().getCurrentWindowMetrics().getBounds();
        Rect expectedNewBoundsInPx =
                new Rect(
                        currentBounds.left + deltaBounds.left,
                        currentBounds.top + deltaBounds.top,
                        currentBounds.right + deltaBounds.right,
                        currentBounds.bottom + deltaBounds.bottom);
        Rect expectedNewBoundsInDp =
                DisplayUtil.scaleToEnclosingRect(
                        expectedNewBoundsInPx, 1.0f / getDipScale(chromeTabbedActivity));
        setBounds(chromeTabbedActivity, expectedNewBoundsInPx);

        // Assert:
        CriteriaHelper.pollUiThread(
                () ->
                        areBoundsCloseEnough(
                                expectedNewBoundsInDp, chromeAndroidTask.getBoundsInDp()),
                /* maxTimeoutMs= */ 5000L,
                /* checkIntervalMs= */ 1000L);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void setBoundsInDp_setsCorrectBounds() {
        assumeBrowserRole();
        // Arrange: Launch ChromeTabbedActivity and find its ChromeAndroidTask.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeTabbedActivity = webPageStation.getActivity();
        var chromeAndroidTask = getChromeAndroidTask(chromeTabbedActivity.getTaskId());
        assertNotNull(chromeAndroidTask);

        Rect currentBoundsInDp =
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getBoundsInDp);
        Rect newBoundsInDp =
                new Rect(
                        currentBoundsInDp.left + 50,
                        currentBoundsInDp.top + 50,
                        currentBoundsInDp.right - 50,
                        currentBoundsInDp.bottom - 50);

        // Act: Call setBoundsInDp.
        ThreadUtils.runOnUiThreadBlocking(() -> chromeAndroidTask.setBoundsInDp(newBoundsInDp));

        // Assert: Verify that the new bounds are applied.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            chromeAndroidTask
                                    .getPendingActionManagerForTesting()
                                    .getFutureBoundsInDp(),
                            Matchers.nullValue());
                    assertBoundsCloseEnoughInDp(newBoundsInDp, chromeAndroidTask.getBoundsInDp());
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void maximize_maximizesTask() throws Exception {
        assumeBrowserRole();
        // Arrange: Launch ChromeTabbedActivity and find its ChromeAndroidTask.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeTabbedActivity = webPageStation.getActivity();
        var chromeAndroidTask = getChromeAndroidTask(chromeTabbedActivity.getTaskId());
        assertNotNull(chromeAndroidTask);

        // Act: Call maximize.
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::maximize);

        // Assert: Verify that the task is maximized.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            chromeAndroidTask
                                    .getPendingActionManagerForTesting()
                                    .isMaximizedFuture(chromeAndroidTask.getState()),
                            Matchers.nullValue());

                    Criteria.checkThat(
                            ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::isMaximized),
                            Matchers.is(true));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    @DisabledTest(message = "https://crbug.com/565864749")
    public void restore_restoresTaskBounds() {
        assumeBrowserRole();
        // Arrange: Launch ChromeTabbedActivity and find its ChromeAndroidTask.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        var chromeTabbedActivity = webPageStation.getActivity();
        var chromeAndroidTask = getChromeAndroidTask(chromeTabbedActivity.getTaskId());
        assertNotNull(chromeAndroidTask);

        Rect currentBoundsInDp =
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getBoundsInDp);
        Rect currentBoundsInPx =
                chromeTabbedActivity.getWindowManager().getCurrentWindowMetrics().getBounds();
        Rect newBoundsInDp =
                new Rect(
                        currentBoundsInDp.left + 50,
                        currentBoundsInDp.top + 50,
                        currentBoundsInDp.right - 50,
                        currentBoundsInDp.bottom - 50);

        // Set specific bounds first.
        ThreadUtils.runOnUiThreadBlocking(() -> chromeAndroidTask.setBoundsInDp(newBoundsInDp));

        // Wait for bounds to be applied so that WindowStateManager updates restored bounds.
        CriteriaHelper.pollUiThread(
                () -> {
                    assertBoundsCloseEnoughInDp(newBoundsInDp, chromeAndroidTask.getBoundsInDp());
                });

        // Maximize it.
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::maximize);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            chromeAndroidTask
                                    .getPendingActionManagerForTesting()
                                    .isMaximizedFuture(chromeAndroidTask.getState()),
                            Matchers.nullValue());
                    Criteria.checkThat(
                            chromeTabbedActivity
                                    .getWindowManager()
                                    .getCurrentWindowMetrics()
                                    .getBounds(),
                            Matchers.not(currentBoundsInPx));
                });

        // Act: Call restore.
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::restore);

        // Assert: Verify that the bounds are restored.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            chromeAndroidTask
                                    .getPendingActionManagerForTesting()
                                    .getFutureBoundsInDp(),
                            Matchers.nullValue());
                    assertBoundsCloseEnoughInDp(newBoundsInDp, chromeAndroidTask.getBoundsInDp());
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void createPendingTask_withInitialBounds_createsTaskWithCorrectBounds() {
        assumeBrowserRole();
        // Arrange: Start on blank page to have a profile.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = assumeNonNull(webPageStation.getTab().getProfile());
        Rect initialBoundsInDp = new Rect(100, 100, 500, 500);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        initialBoundsInDp.left,
                        initialBoundsInDp.top,
                        initialBoundsInDp.right,
                        initialBoundsInDp.bottom,
                        WindowShowState.DEFAULT,
                        /* webContents= */ null);

        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(ChromeAndroidTaskTrackerImpl::getInstance);
        assertNotNull(chromeAndroidTaskTracker);

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Act: Create pending task and keep the reference.
        var chromeAndroidTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTaskTracker.createPendingTask(createParams, null));
        assertNotNull(chromeAndroidTask);

        // Wait for the new activity to be created.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);

        // Assert: Verify that the new activity has the correct bounds using the kept reference.
        CriteriaHelper.pollUiThread(
                () ->
                        assertBoundsCloseEnoughInDp(
                                initialBoundsInDp, chromeAndroidTask.getBoundsInDp()));

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void createPendingTask_withInitialShowStateAsMaximized_createsTaskWithMaximizedBounds() {
        assumeBrowserRole();
        // Arrange.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = assumeNonNull(webPageStation.getTab().getProfile());

        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(ChromeAndroidTaskTrackerImpl::getInstance);
        assertNotNull(chromeAndroidTaskTracker);

        // Act: Create a pending Task with the initialShowState as maximized.
        var newTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AndroidBrowserWindowCreateParams createParams =
                                    AndroidBrowserWindowCreateParamsImpl.create(
                                            BrowserWindowType.NORMAL,
                                            profile,
                                            /* leftBound= */ 0,
                                            /* topBound= */ 0,
                                            /* rightBound= */ 0,
                                            /* bottomBound= */ 0,
                                            /* initialShowState= */ WindowShowState.MAXIMIZED,
                                            /* webContents= */ null);
                            return chromeAndroidTaskTracker.createPendingTask(
                                    createParams, /* callback= */ null);
                        });
        assertNotNull(newTask);

        // Assert:
        //
        // (1) Wait for the pending Task to become idle, which means the pending Task has been
        // backed by a real Activity;
        // (2) The Task reports it's in the maximized state.
        //
        // Note: we should wait longer than CriteriaHelper's default timeout since new Task/Activity
        // creation takes time.
        CriteriaHelper.pollUiThread(
                () ->
                        newTask.getState() == ChromeAndroidTaskImpl.State.IDLE
                                && newTask.isMaximized(),
                /* maxTimeoutMs= */ 10_000L,
                /* checkIntervalMs= */ 1_000L);

        // Assert:
        // The new Task's top Activity has maximized bounds.
        CriteriaHelper.pollUiThread(
                () -> {
                    var newActivityWindowAndroid = newTask.getTopActivityWindowAndroid();
                    assertNotNull(newActivityWindowAndroid);

                    var newActivity = newActivityWindowAndroid.getActivity().get();
                    assertNotNull(newActivity);

                    var windowManager = newActivity.getWindowManager();
                    var currentBounds = windowManager.getCurrentWindowMetrics().getBounds();
                    var maximizedBounds =
                            ChromeAndroidTaskBoundsConstraints.getMaxBoundsInPx(windowManager);
                    assertEquals(maximizedBounds, currentBounds);
                });

        // Cleanup.
        ThreadUtils.runOnUiThreadBlocking(newTask::close);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void createPendingTask_requestSetBounds_dispatchesSetBounds() {
        assumeBrowserRole();
        // Arrange: Start on blank page to have a profile.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = assumeNonNull(webPageStation.getTab().getProfile());
        Rect initialBoundsInDp = new Rect(100, 100, 500, 500);
        Rect newBoundsInDp = new Rect(200, 200, 600, 600);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        initialBoundsInDp.left,
                        initialBoundsInDp.top,
                        initialBoundsInDp.right,
                        initialBoundsInDp.bottom,
                        WindowShowState.DEFAULT,
                        /* webContents= */ null);

        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(ChromeAndroidTaskTrackerImpl::getInstance);
        assertNotNull(chromeAndroidTaskTracker);

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Pause pending task activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                chromeAndroidTaskTracker::pausePendingTaskActivityCreationForTesting);

        // Act: Create pending task.
        var chromeAndroidTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTaskTracker.createPendingTask(createParams, null));
        assertNotNull(chromeAndroidTask);

        var pendingTaskInfo =
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getPendingTaskInfo);
        assertNotNull(pendingTaskInfo);

        // Request setBounds on pending task.
        ThreadUtils.runOnUiThreadBlocking(() -> chromeAndroidTask.setBoundsInDp(newBoundsInDp));

        // Resume activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        chromeAndroidTaskTracker.resumePendingTaskActivityCreationForTesting(
                                pendingTaskInfo.mPendingTaskId));

        // Assert: Verify that the new activity has the requested bounds (newBoundsInDp, not
        // initialBoundsInDp).
        var newActivity = waitForNewTabbedActivity(currentTaskIds);

        CriteriaHelper.pollUiThread(
                () ->
                        assertBoundsCloseEnoughInDp(
                                newBoundsInDp, chromeAndroidTask.getBoundsInDp()));

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void createPendingTask_requestMaximize_dispatchesMaximize() throws Exception {
        assumeBrowserRole();
        // Arrange: Start on blank page to have a profile.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = assumeNonNull(webPageStation.getTab().getProfile());
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        /* leftBound= */ 0,
                        /* topBound= */ 0,
                        /* rightBound= */ 0,
                        /* bottomBound= */ 0,
                        WindowShowState.DEFAULT,
                        /* webContents= */ null);

        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(ChromeAndroidTaskTrackerImpl::getInstance);
        assertNotNull(chromeAndroidTaskTracker);

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Pause pending task activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                chromeAndroidTaskTracker::pausePendingTaskActivityCreationForTesting);

        // Act: Create pending task.
        var chromeAndroidTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTaskTracker.createPendingTask(createParams, null));
        assertNotNull(chromeAndroidTask);

        var pendingTaskInfo =
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getPendingTaskInfo);
        assertNotNull(pendingTaskInfo);

        // Request maximize on pending task.
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::maximize);

        // Resume activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        chromeAndroidTaskTracker.resumePendingTaskActivityCreationForTesting(
                                pendingTaskInfo.mPendingTaskId));

        // Assert: Verify that the new activity is maximized.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(chromeAndroidTask.isMaximized(), Matchers.is(true)));

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void createPendingTask_requestMinimize_dispatchesMinimize() throws Exception {
        assumeBrowserRole();
        // Arrange: Start on blank page to have a profile.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = assumeNonNull(webPageStation.getTab().getProfile());
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        /* leftBound= */ 0,
                        /* topBound= */ 0,
                        /* rightBound= */ 0,
                        /* bottomBound= */ 0,
                        WindowShowState.DEFAULT,
                        /* webContents= */ null);

        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(ChromeAndroidTaskTrackerImpl::getInstance);
        assertNotNull(chromeAndroidTaskTracker);

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Pause pending task activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                chromeAndroidTaskTracker::pausePendingTaskActivityCreationForTesting);

        // Act: Create pending task.
        var chromeAndroidTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTaskTracker.createPendingTask(createParams, null));
        assertNotNull(chromeAndroidTask);

        var pendingTaskInfo =
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getPendingTaskInfo);
        assertNotNull(pendingTaskInfo);

        // Request minimize on pending task.
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::minimize);

        // Intercept moveTaskToBack to verify minimize.
        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();

        // Resume activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        chromeAndroidTaskTracker.resumePendingTaskActivityCreationForTesting(
                                pendingTaskInfo.mPendingTaskId));

        // Assert: Verify that the new activity is minimized (moveTaskToBack intercepted).
        var newActivity = waitForNewTabbedActivity(currentTaskIds);

        CriteriaHelper.pollUiThread(
                ChromeTabbedActivity::wasMoveTaskToBackInterceptedForTesting,
                "Failed to move task to the background.");

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void createPendingTask_requestSetBoundsAndDeactivate_dispatchesBoth() {
        assumeBrowserRole();
        // Arrange: Start on blank page to have a profile.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        Profile profile = assumeNonNull(webPageStation.getTab().getProfile());
        Rect initialBoundsInDp = new Rect(100, 100, 500, 500);
        Rect newBoundsInDp = new Rect(200, 200, 600, 600);
        AndroidBrowserWindowCreateParams createParams =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        profile,
                        initialBoundsInDp.left,
                        initialBoundsInDp.top,
                        initialBoundsInDp.right,
                        initialBoundsInDp.bottom,
                        WindowShowState.DEFAULT,
                        /* webContents= */ null);

        var chromeAndroidTaskTracker =
                ThreadUtils.runOnUiThreadBlocking(ChromeAndroidTaskTrackerImpl::getInstance);
        assertNotNull(chromeAndroidTaskTracker);

        Set<Integer> currentTaskIds = getTabbedActivityTaskIds();

        // Pause pending task activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                chromeAndroidTaskTracker::pausePendingTaskActivityCreationForTesting);

        // Act: Create pending task.
        var chromeAndroidTask =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> chromeAndroidTaskTracker.createPendingTask(createParams, null));
        assertNotNull(chromeAndroidTask);

        var pendingTaskInfo =
                ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::getPendingTaskInfo);
        assertNotNull(pendingTaskInfo);

        // Request setBounds on pending task.
        ThreadUtils.runOnUiThreadBlocking(() -> chromeAndroidTask.setBoundsInDp(newBoundsInDp));

        // Request deactivate on pending task.
        ThreadUtils.runOnUiThreadBlocking(chromeAndroidTask::deactivate);

        // Resume activity creation.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        chromeAndroidTaskTracker.resumePendingTaskActivityCreationForTesting(
                                pendingTaskInfo.mPendingTaskId));

        // Assert: Verify that the new activity has the requested bounds and is not active.
        var newActivity = waitForNewTabbedActivity(currentTaskIds);
        var windowAndroid = newActivity.getWindowAndroid();
        assertNotNull(windowAndroid);

        CriteriaHelper.pollUiThread(
                () -> {
                    assertBoundsCloseEnoughInDp(newBoundsInDp, chromeAndroidTask.getBoundsInDp());
                    Criteria.checkThat(windowAndroid.isTopResumedActivity(), Matchers.is(false));
                });

        // Cleanup.
        newActivity.finishAndRemoveTask();
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private static void assumeBrowserRole() {
        assumeTrue(
                "The test suite requires the APK to be the default browser. "
                        + "Please run "
                        + "'adb shell cmd role add-role-holder android.app.role.BROWSER "
                        + ContextUtils.getApplicationContext().getPackageName()
                        + "'",
                hasBrowserRole());
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private static boolean hasBrowserRole() {
        Context appContext = ContextUtils.getApplicationContext();
        var roleManager = appContext.getSystemService(RoleManager.class);
        return roleManager.isRoleHeld(RoleManager.ROLE_BROWSER);
    }

    private float getDipScale(ChromeTabbedActivity chromeTabbedActivity) {
        var activityWindowAndroid = chromeTabbedActivity.getWindowAndroid();
        assertNotNull(activityWindowAndroid);

        return activityWindowAndroid.getDisplay().getDipScale();
    }

    private static boolean areBoundsCloseEnough(Rect expected, Rect actual) {
        return Math.abs(actual.left - expected.left) <= BOUNDS_CHECK_TOLERANCE_DP
                && Math.abs(actual.top - expected.top) <= BOUNDS_CHECK_TOLERANCE_DP
                && Math.abs(actual.right - expected.right) <= BOUNDS_CHECK_TOLERANCE_DP
                && Math.abs(actual.bottom - expected.bottom) <= BOUNDS_CHECK_TOLERANCE_DP;
    }

    private static void assertBoundsCloseEnoughInDp(Rect expected, Rect actual) {
        assertTrue(
                String.format(
                        Locale.US,
                        "Bounds not close enough. Expected: %s; Actual: %s",
                        expected,
                        actual),
                areBoundsCloseEnough(expected, actual));
    }

    private AppTask getAppTask(Activity activity) {
        var appTaskForActivity = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        assertNotNull(appTaskForActivity);
        return appTaskForActivity;
    }

    private int getDisplayId(ChromeTabbedActivity chromeTabbedActivity) {
        var windowAndroid = chromeTabbedActivity.getWindowAndroid();
        assertNotNull(windowAndroid);

        return windowAndroid.getDisplay().getDisplayId();
    }

    /**
     * Sets the bounds of the given {@link ChromeTabbedActivity}'s Task.
     *
     * <p>The method won't return until the new bounds are applied or the operation fails.
     */
    @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
    private void setBounds(ChromeTabbedActivity chromeTabbedActivity, Rect newBounds)
            throws Exception {
        var callBackHelper = new CallbackHelper();

        OutcomeReceiver<TaskLocation, Exception> listener =
                new OutcomeReceiver<>() {
                    @Override
                    public void onError(Exception e) {
                        callBackHelper.notifyFailed(e.toString());
                    }

                    @Override
                    public void onResult(TaskLocation tl) {
                        callBackHelper.notifyCalled();
                    }
                };
        getAppTask(chromeTabbedActivity)
                .moveTaskTo(
                        new TaskLocation(getDisplayId(chromeTabbedActivity), newBounds),
                        Runnable::run,
                        listener);

        callBackHelper.waitForCallback(/* currentCallCount= */ 0);
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
                    var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
                    return (ChromeAndroidTaskImpl) chromeAndroidTaskTracker.get(taskId);
                });
    }

    private static final class TestChromeAndroidTaskFeature implements ChromeAndroidTaskFeature {

        final List<Rect> mTaskBoundsChangeDpHistory =
                Collections.synchronizedList(new ArrayList<>());
        final List<Rect> mTaskBoundsChangePxHistory =
                Collections.synchronizedList(new ArrayList<>());
        final List<Boolean> mTaskFocusChangedParams = new ArrayList<>();

        @Override
        public void onAddedToTask(InitInfo initInfo) {}

        @Override
        public void onFeatureRemoved() {}

        @Override
        public void onTaskBoundsChanged(int displayId, Rect newBoundsInDp, Rect newBoundsInPx) {
            mTaskBoundsChangeDpHistory.add(newBoundsInDp);
            mTaskBoundsChangePxHistory.add(newBoundsInPx);
        }

        @Override
        public void onTaskFocusChanged(boolean hasFocus) {
            mTaskFocusChangedParams.add(hasFocus);
        }
    }
}
