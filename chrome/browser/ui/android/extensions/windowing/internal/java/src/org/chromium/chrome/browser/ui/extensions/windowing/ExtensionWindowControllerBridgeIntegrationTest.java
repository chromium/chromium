// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.extensions.windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = Batch.PER_CLASS)
@NullMarked
public class ExtensionWindowControllerBridgeIntegrationTest {

    @Rule
    public FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

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
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @Restriction(
            // Test needs "new window" in app menu and the tablet behavior to enter split screen
            // mode to trigger a window bounds change.
            DeviceFormFactor.ONLY_TABLET)
    public void
            startChromeTabbedActivity_triggerTaskBoundsChange_notifyExtensionWindowController() {
        // Arrange: launch ChromeTabbedActivity (the first window).
        WebPageStation webPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
        int firstTaskId = mFreshCtaTransitTestRule.getActivity().getTaskId();
        var extensionWindowControllerBridge = getExtensionWindowControllerBridge(firstTaskId);
        assertNotNull(extensionWindowControllerBridge);
        extensionWindowControllerBridge.addWindowControllerListObserverForTesting();

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
                extensionWindowControllerBridge.getExtensionInternalEventsForTesting();
        assertEquals(1, extensionInternalEvents.size());
        assertEquals(
                ExtensionInternalWindowEventForTesting.BOUNDS_CHANGED,
                (int) extensionInternalEvents.get(0));

        // Cleanup.
        extensionWindowControllerBridge.removeWindowControllerListObserverForTesting();
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
