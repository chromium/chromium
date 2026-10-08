// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewFinder;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/** Test utilities for Vertical Tabs and tab strip mode interactions. */
public class VerticalTabsTestUtils {

    /**
     * Interface allowing {@link #toggleTabStripForTesting} to be used in try-with-resources or as a
     * {@link Runnable}.
     */
    @FunctionalInterface
    public interface TabStripResetter extends AutoCloseable, Runnable {
        @Override
        void close();

        @Override
        default void run() {
            close();
        }
    }

    /**
     * Toggles the tab strip between horizontal and vertical tabs mode for testing. Registers a
     * resetter with {@link ResettersForTesting} so that the toggle is automatically reverted when
     * the test finishes. Also returns a {@link TabStripResetter} to allow reverting the toggle
     * early via try-with-resources or manual invocation if desired.
     *
     * <p>This method waits for the UI transition between tab strip modes to settle before
     * returning.
     *
     * @param cta The {@link ChromeTabbedActivity} instance.
     * @return A {@link TabStripResetter} to revert the toggle early.
     */
    public static TabStripResetter toggleTabStripForTesting(ChromeTabbedActivity cta) {
        boolean willBeVertical =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> !VerticalTabUtils.isVerticalTabsEnabled(cta));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedRootUiCoordinator rootUiCoordinator =
                            (TabbedRootUiCoordinator) cta.getRootUiCoordinatorForTesting();
                    rootUiCoordinator.toggleTabStrip();
                });
        waitForTabStripState(cta, willBeVertical);

        AtomicBoolean isReset = new AtomicBoolean(false);
        TabStripResetter resetter =
                () -> {
                    if (isReset.compareAndSet(false, true)) {
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    if (cta == null || cta.isActivityFinishingOrDestroyed()) {
                                        return;
                                    }
                                    TabbedRootUiCoordinator rootUiCoordinator =
                                            (TabbedRootUiCoordinator)
                                                    cta.getRootUiCoordinatorForTesting();
                                    if (rootUiCoordinator != null) {
                                        rootUiCoordinator.toggleTabStrip();
                                    }
                                });
                        if (cta != null && !cta.isActivityFinishingOrDestroyed()) {
                            waitForTabStripState(cta, !willBeVertical);
                        }
                    }
                };
        ResettersForTesting.register(resetter);
        return resetter;
    }

    private static void waitForTabStripState(ChromeTabbedActivity cta, boolean expectVertical) {
        Matcher<View> railMatcher = withId(R.id.vertical_tab_rail_container);
        if (expectVertical) {
            ViewFinder.waitForView(cta, railMatcher);
        } else {
            ViewFinder.waitForNoView(cta, railMatcher);
        }
    }
}
