// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

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
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(value = Batch.PER_CLASS)
@NullMarked
public class ChromeAndroidTaskIntegrationTest {

    @Rule
    public FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

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
