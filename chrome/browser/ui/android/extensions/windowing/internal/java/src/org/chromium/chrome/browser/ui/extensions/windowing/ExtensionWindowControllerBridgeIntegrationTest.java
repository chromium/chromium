// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.extensions.windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;

import java.util.List;

@RunWith(BaseJUnit4ClassRunner.class)
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

    private @Nullable ExtensionWindowControllerBridgeImpl getExtensionWindowControllerBridge(
            int taskId) {
        var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
        assertNotNull(chromeAndroidTaskTracker);

        var chromeAndroidTask = chromeAndroidTaskTracker.get(taskId);
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
