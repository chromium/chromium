// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.extensions.windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
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
import java.util.List;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(value = Batch.PER_CLASS)
@CommandLineFlags.Add({
    // Force DeviceInfo#isDesktop() to be true so that the DISABLE_INSTANCE_LIMIT
    // flag in @EnableFeatures can be effective when running tests on an
    // emulator without "--force-desktop-android".
    //
    // See MultiWindowUtils#getMaxInstances() for the reason:
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java;l=213;drc=0bcba72c5246a910240b311def40233f7d3f15af
    BaseSwitches.FORCE_DESKTOP_ANDROID,
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE
})
@Features.EnableFeatures({
    // Disable ChromeTabbedActivity instance limit so that the total number of
    // windows created by the entire test suite won't be limited.
    //
    // See MultiWindowUtils#getMaxInstances() for the reason:
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java;l=209;drc=0bcba72c5246a910240b311def40233f7d3f15af
    ChromeFeatureList.DISABLE_INSTANCE_LIMIT,
    ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
})
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
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
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
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
        assertNotNull(extensionWindowControllerBridge);
    }

    @Test
    @MediumTest
    public void startWebappActivity_addsExtensionWindowControllerBridgeToChromeAndroidTask() {
        // Act.
        mWebappActivityTestRule.startWebappActivity();

        // Assert.
        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
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
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

        // Act: Open a new window.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(secondTaskId);
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
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
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
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();
        IncognitoNewTabPageStation incognitoNtpStation =
                blankPageStation.openRegularTabAppMenu().openNewIncognitoWindow();
        int incognitoTaskId = incognitoNtpStation.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(incognitoTaskId);
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

        // Cleanup.
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
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
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

        // Act: Start CustomTabActivity as a popup window (the second window)
        var customTabIntent = createCustomTabIntent(CustomTabsUiType.POPUP);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);
        int secondTaskId = mCustomTabActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(secondTaskId);
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
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
        mCustomTabActivityTestRule.getActivity().finish();
    }

    @Test
    @MediumTest
    public void startWebappActivity_notifyExtensionInternalsOfWindowCreation() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity. We need this to initialize native
        // libraries, which is a prerequisite for Step (2).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        mFreshCtaTransitTestRule.startOnBlankPage();
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

        // Act: Start WebappActivity.
        mWebappActivityTestRule.startWebappActivity();
        int webappTaskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(webappTaskId);
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
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
        mWebappActivityTestRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @Restriction(
            // Test needs "new window" in app menu and the tablet behavior to enter split screen
            // mode to trigger a window bounds change.
            DeviceFormFactor.ONLY_TABLET)
    @Features.DisableFeatures(
            // When ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL is enabled, a new window will be full
            // screen instead of being in the split screen mode. This test relies on the split
            // screen mode to trigger task bounds change, so
            // ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL needs to be disabled.
            ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void
            startChromeTabbedActivity_triggerTaskBoundsChange_notifyExtensionWindowController() {
        // Arrange:
        // (1) Launch ChromeTabbedActivity (the first window).
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(firstTaskId);
        assertNotNull(extensionWindowControllerBridge);
        int firstExtensionWindowId =
                extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

        // Act: Open a new window.
        // On tablets, this will enter split screen mode and trigger a window bounds change for the
        // first window.
        RegularNewTabPageStation ntpStation =
                webPageStation.openRegularTabAppMenu().openNewWindow();
        int secondTaskId = ntpStation.getActivity().getTaskId();
        var secondChromeAndroidTask = getChromeAndroidTask(secondTaskId);
        assertNotNull(secondChromeAndroidTask);
        CriteriaHelper.pollUiThread(secondChromeAndroidTask::isActive);

        // Assert.
        var extensionInternalEvents =
                ExtensionWindowControllerBridgeImpl.getExtensionInternalEventsForTesting()
                        .get(firstExtensionWindowId);
        assertNotNull(extensionInternalEvents);
        assertTrue(
                extensionInternalEvents.contains(
                        ExtensionInternalWindowEventForTesting.BOUNDS_CHANGED));

        // Cleanup.
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
        ntpStation.getActivity().finish();
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
        var firstExtensionWindowControllerBridge = getExtensionWindowControllerBridge(firstTaskId);
        assertNotNull(firstExtensionWindowControllerBridge);
        int firstExtensionWindowId =
                firstExtensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

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
                getExtensionWindowControllerBridge(secondTaskId);
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
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
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
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
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
                getExtensionWindowControllerBridge(chromeTabbedActivityTaskId);
        var customTabExtensionWindowControllerBridge =
                getExtensionWindowControllerBridge(customTabTaskId);

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
    public void destroyWebappActivity_destroysExtensionWindowControllerBridge() {
        // Arrange.
        mWebappActivityTestRule.startWebappActivity();
        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
        assertNotNull(extensionWindowControllerBridge);
        assertNotEquals(0, extensionWindowControllerBridge.getNativePtrForTesting());

        // Act.
        mWebappActivityTestRule.finishActivity();

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
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

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

        // Cleanup.
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
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
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

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

        // Cleanup.
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
    }

    @Test
    @MediumTest
    public void destroyWebActivity_notifyExtensionInternalsOfWindowDestruction() {
        // Arrange:
        // (1) Start WebappActivity.
        // (2) Add a native WindowControllerListObserverForTesting to capture extension internal
        // events.
        mWebappActivityTestRule.startWebappActivity();

        int taskId = mWebappActivityTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(taskId);
        assertNotNull(extensionWindowControllerBridge);
        var extensionWindowId = extensionWindowControllerBridge.getExtensionWindowIdForTesting();
        ExtensionWindowControllerBridgeImpl.addWindowControllerListObserverForTesting();

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

        // Cleanup.
        ExtensionWindowControllerBridgeImpl.removeWindowControllerListObserverForTesting();
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

    private @Nullable ExtensionWindowControllerBridgeImpl getExtensionWindowControllerBridge(
            int taskId) {
        var chromeAndroidTask = getChromeAndroidTask(taskId);
        assertNotNull(chromeAndroidTask);

        List<ChromeAndroidTaskFeature> features = chromeAndroidTask.getAllFeaturesForTesting();
        if (features.isEmpty()) {
            return null;
        }

        // Note:
        //
        // As of July 24, 2025, ExtensionWindowControllerBridge is the only
        // ChromeAndroidTaskFeature, so if the feature list is not empty, it must contain
        // exactly one ExtensionWindowControllerBridge instance.
        //
        // TODO(crbug.com/434055958): use the new feature lookup API in ChromeAndroidTask to
        // retrieve ExtensionWindowControllerBridge.
        assertTrue(features.size() == 1);
        var chromeAndroidTaskFeature = features.get(0);
        if (!(chromeAndroidTaskFeature instanceof ExtensionWindowControllerBridgeImpl)) {
            return null;
        }

        return (ExtensionWindowControllerBridgeImpl) chromeAndroidTaskFeature;
    }
}
