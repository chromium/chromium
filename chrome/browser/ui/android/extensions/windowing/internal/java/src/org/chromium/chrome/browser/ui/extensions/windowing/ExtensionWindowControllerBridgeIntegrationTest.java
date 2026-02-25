// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.extensions.windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Intent;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTypeTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Collections;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(value = Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
@MinAndroidSdkLevel(Build.VERSION_CODES.R)
@NullMarked
public class ExtensionWindowControllerBridgeIntegrationTest {

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public WebappActivityTestRule mWebappActivityTestRule = new WebappActivityTestRule();

    @Test
    @MediumTest
    public void startChromeTabbedActivity_addsExtensionWindowControllerBridgeToChromeAndroidTask() {
        // Arrange & Act.
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();

        // Assert.
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mFreshCtaTransitTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
    }

    @Test
    @MediumTest
    public void
            startCustomTabActivityAsPopup_addsExtensionWindowControllerBridgeToChromeAndroidTask() {
        // Arrange.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.POPUP);

        // Act.
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // Assert.
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mCustomTabActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void startWebappActivity_addsExtensionWindowControllerBridgeToChromeAndroidTask() {
        // Act.
        mWebappActivityTestRule.startWebappActivity();

        // Assert.
        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mWebappActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void startTwa_addsExtensionWindowControllerBridgeToChromeAndroidTask() throws Exception {
        // Act.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");

        // Assert.
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mCustomTabActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* Test needs "new window" in app menu. */)
    public void startChromeTabbedActivity_openNewWindow_notifyExtensionInternalsOfWindowCreation() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity (the first window).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act: Open a new window.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(secondTaskId, ntpStation.getTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.CREATED,
                (int) extensionInternalEvents.get(0));

        // Cleanup.
        ntpStation.getActivity().finish();
    }

    /**
     * Tests the short-term fix for <a
     * href="http://crbug.com/450234852">http://crbug.com/450234852</a>.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* Test needs "new window" in app menu. */)
    public void
            openIncognitoWindow_destroyIncognitoTabModel_notifyExtensionInternalsOfWindowDestruction() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity (the first window).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        // (3) Open an incognito window.
        WebPageStation blankPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();
        IncognitoNewTabPageStation incognitoNtpStation =
                blankPageStation.openRegularTabAppMenu().openNewIncognitoWindow();
        int incognitoTaskId = incognitoNtpStation.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        incognitoTaskId, incognitoNtpStation.getTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        int incognitoExtensionWindowId =
                extensionWindowControllerBridge.getExtensionWindowIdForTesting();

        // Act:
        // (1) Destroy incognito tab model.
        // (2) Wait for the incognito window to be destroyed.
        ThreadUtils.runOnUiThreadBlocking(IncognitoTabHostUtils::closeAllIncognitoTabs);
        CriteriaHelper.pollUiThread(
                () -> getChromeAndroidTask(incognitoTaskId) == null,
                /* maxTimeoutMs= */ 10000L,
                /* checkIntervalMs= */ 1000L);

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(incognitoExtensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.REMOVED,
                (int) extensionInternalEvents.get(extensionInternalEvents.size() - 1));
    }

    @Test
    @MediumTest
    public void
            startChromeTabbedActivity_startCustomTabActivityAsPopup_notifyExtensionInternalsOfWindowCreation() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity (the first window).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act: Start CustomTabActivity as a popup window (the second window)
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.POPUP);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);
        int secondTaskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        secondTaskId, mCustomTabActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.CREATED,
                (int) extensionInternalEvents.get(0));

        // Cleanup.
        mCustomTabActivityTestRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void startWebappActivity_notifyExtensionInternalsOfWindowCreation() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity. We need this to initialize native
        // libraries, which is a prerequisite for Step (2).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act: Start WebappActivity.
        mWebappActivityTestRule.startWebappActivity();
        int webappTaskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        webappTaskId, mWebappActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.CREATED,
                (int) extensionInternalEvents.get(0));

        // Cleanup.
        mWebappActivityTestRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void startTwa_notifyExtensionInternalsOfWindowCreation() throws Exception {
        // Arrange:
        // (1) Launch ChromeTabbedActivity. We need this to initialize native
        // libraries, which is a prerequisite for Step (2).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act: Start TWA.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");

        int twaTaskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        twaTaskId, mCustomTabActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.CREATED,
                (int) extensionInternalEvents.get(0));

        // Cleanup.
        mCustomTabActivityTestRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP /* Test needs "new window" in app menu. */)
    public void startChromeTabbedActivity_triggerTaskFocusChange_notifyExtensionWindowController() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity (the first window).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var firstExtensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        firstTaskId, webPageStation.getTab().getProfile());
        assertNotNull(firstExtensionWindowControllerBridge);
        int firstExtensionWindowId =
                firstExtensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act: Open a new window.
        // This will cause the first window to lose focus and the second window to gain focus.
        // Both focus change events should be captured.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);
        CriteriaHelper.pollUiThread(secondChromeAndroidTask::isActive);
        var secondExtensionWindowControllerBridge =
                getExtensionWindowControllerBridge(secondTaskId, ntpStation.getTab().getProfile());
        assertNotNull(secondExtensionWindowControllerBridge);
        var secondExtensionWindowId =
                secondExtensionWindowControllerBridge.getExtensionWindowIdForTesting();

        // Assert.
        var extensionInternalEventsForFirstWindow =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(firstExtensionWindowId);
        assertNotNull(extensionInternalEventsForFirstWindow);
        var extensionInternalEventsForSecondWindow =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(secondExtensionWindowId);
        assertNotNull(extensionInternalEventsForSecondWindow);
        assertEquals(
                1,
                Collections.frequency(
                        extensionInternalEventsForFirstWindow,
                        ExtensionInternalWindowEventForTesting.FOCUS_LOST));
        assertEquals(
                1,
                Collections.frequency(
                        extensionInternalEventsForSecondWindow,
                        ExtensionInternalWindowEventForTesting.FOCUS_OBTAINED));

        // Cleanup.
        ntpStation.getActivity().finish();
    }

    /**
     * Verifies that an {@link ExtensionWindowControllerBridge} is destroyed with its {@code
     * Activity}.
     *
     * <p>This is the right behavior when {@link ChromeAndroidTask} tracks an {@code Activity},
     * which is a workaround to track a Task (window).
     *
     * <p>If {@link ChromeAndroidTask} tracks a Task, {@link ExtensionWindowControllerBridge} should
     * continue to exist as long as the Task is alive.
     *
     * <p>Please see the documentation of {@link ChromeAndroidTask} for details.
     */
    @Test
    @MediumTest
    public void destroyChromeTabbedActivity_destroysExtensionWindowControllerBridge() {
        // Arrange.
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mFreshCtaTransitTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        assertNotEquals(0, extensionWindowControllerBridge.getNativePtrForTesting());

        // Act.
        mFreshCtaTransitTestRule.finishActivity();

        // Assert.
        assertEquals(0, extensionWindowControllerBridge.getNativePtrForTesting());
    }

    @Test
    @MediumTest
    public void destroyPopupCustomTabActivity_destroysExtensionWindowControllerBridgeForPopup() {
        // Arrange: start a CustomTabActivity as a popup window on top of a ChromeTabbedActivity
        mFreshCtaTransitTestRule.startOnBlankPage();
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.POPUP);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        int chromeTabbedActivityTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        int customTabTaskId = mCustomTabActivityTestRule.getActivity().getTaskId();

        var chromeTabbedActivityExtensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        chromeTabbedActivityTaskId,
                        mFreshCtaTransitTestRule.getActivityTab().getProfile());
        var customTabExtensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        customTabTaskId, mCustomTabActivityTestRule.getActivityTab().getProfile());

        assertNotNull(chromeTabbedActivityExtensionWindowControllerBridge);
        assertNotNull(customTabExtensionWindowControllerBridge);
        assertNotEquals(
                0, chromeTabbedActivityExtensionWindowControllerBridge.getNativePtrForTesting());
        assertNotEquals(0, customTabExtensionWindowControllerBridge.getNativePtrForTesting());

        // Act.
        mCustomTabActivityTestRule.finishActivity();

        // Assert.
        assertNotEquals(
                0, chromeTabbedActivityExtensionWindowControllerBridge.getNativePtrForTesting());
        assertEquals(0, customTabExtensionWindowControllerBridge.getNativePtrForTesting());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void destroyWebappActivity_destroysExtensionWindowControllerBridge() {
        // Arrange.
        mWebappActivityTestRule.startWebappActivity();
        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mWebappActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        assertNotEquals(0, extensionWindowControllerBridge.getNativePtrForTesting());

        // Act.
        mWebappActivityTestRule.finishActivity();

        // Assert.
        assertEquals(0, extensionWindowControllerBridge.getNativePtrForTesting());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void destroyTwa_destroysExtensionWindowControllerBridge() throws Exception {
        // Arrange.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");
        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mCustomTabActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        assertNotEquals(0, extensionWindowControllerBridge.getNativePtrForTesting());

        // Act.
        mCustomTabActivityTestRule.finishActivity();

        // Assert.
        assertEquals(0, extensionWindowControllerBridge.getNativePtrForTesting());
    }

    @Test
    @MediumTest
    public void destroyChromeTabbedActivity_notifyExtensionInternalsOfWindowDestruction() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity (the first window).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        mFreshCtaTransitTestRule.startOnBlankPage();
        int taskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mFreshCtaTransitTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act.
        mFreshCtaTransitTestRule.finishActivity();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.REMOVED,
                (int) extensionInternalEvents.get(extensionInternalEvents.size() - 1));
    }

    @Test
    @MediumTest
    public void destroyPopupCustomTabActivity_notifyExtensionInternalsOfWindowDestruction() {
        // Arrange:
        // (1) Start CustomTabActivity as a popup window.
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.POPUP);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mCustomTabActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act.
        mCustomTabActivityTestRule.finishActivity();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.REMOVED,
                (int) extensionInternalEvents.get(extensionInternalEvents.size() - 1));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void destroyWebActivity_notifyExtensionInternalsOfWindowDestruction() {
        // Arrange:
        // (1) Start WebappActivity.
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        mWebappActivityTestRule.startWebappActivity();

        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mWebappActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act.
        mWebappActivityTestRule.finishActivity();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.REMOVED,
                (int) extensionInternalEvents.get(extensionInternalEvents.size() - 1));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/486858979: Temporarily disabled to avoid crashes.")
    public void destroyTwa_notifyExtensionInternalsOfWindowDestruction() throws Exception {
        // Arrange:
        // (1) Start TWA.
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        CustomTabActivityTypeTestUtils.launchActivity(
                ActivityType.TRUSTED_WEB_ACTIVITY, mCustomTabActivityTestRule, "about:blank");

        int taskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge =
                getExtensionWindowControllerBridge(
                        taskId, mCustomTabActivityTestRule.getActivityTab().getProfile());
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.initializeWindowControllerListObserverForTesting();

        // Act.
        mCustomTabActivityTestRule.finishActivity();

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(extensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertEquals(
                ExtensionInternalWindowEventForTesting.REMOVED,
                (int) extensionInternalEvents.get(extensionInternalEvents.size() - 1));
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
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
                    assertNotNull(chromeAndroidTaskTracker);

                    return chromeAndroidTaskTracker.get(taskId);
                });
    }

    private @Nullable ExtensionWindowControllerBridgeImpl getExtensionWindowControllerBridge(
            int taskId, @Nullable Profile profile) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var chromeAndroidTask = getChromeAndroidTask(taskId);
                    assertNotNull(chromeAndroidTask);

                    var activityWindowAndroid = chromeAndroidTask.getTopActivityWindowAndroid();
                    return (ExtensionWindowControllerBridgeImpl)
                            chromeAndroidTask.getFeatureForTesting(
                                    new ChromeAndroidTaskFeatureKey(
                                            ExtensionWindowControllerBridge.class,
                                            profile,
                                            activityWindowAndroid));
                });
    }
}
