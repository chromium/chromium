// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.batch;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * To be used by batched tests that would like to reset to a single blank tab open in
 * ChromeTabbedActivity between each test, without restarting the Activity.
 *
 * State is stored statically, and so the Activity may be reused across multiple test suites within
 * the same {@link Batch}.
 *
 * @param <T> The type of {@link ChromeActivity}.
 */
public class BlankCTATabInitialStateRule<T extends ChromeActivity> implements TestRule {
    private static ChromeActivity sActivity;

    private final ChromeActivityTestRule<T> mActivityTestRule;
    private final boolean mClearAllTabState;

    /**
     * @param activityTestRule The ActivityTestRule to be reset to initial state between each test.
     * @param clearAllTabState More thoroughly clears all tab state, at the cost of restarting the
     *     renderer process between each test.
     */
    public BlankCTATabInitialStateRule(
            ChromeActivityTestRule<T> activityTestRule, boolean clearAllTabState) {
        super();
        mActivityTestRule = activityTestRule;
        mClearAllTabState = clearAllTabState;
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                if (sActivity == null) {
                    TestThreadUtils.runOnUiThreadBlocking(
                            () -> { FirstRunStatus.setFirstRunFlowComplete(true); });
                    mActivityTestRule.startMainActivityOnBlankPage();
                    sActivity = mActivityTestRule.getActivity();
                } else {
                    mActivityTestRule.setActivity((T) sActivity);
                    if (mClearAllTabState) {
                        resetTabStateThorough();
                    } else {
                        resetTabStateFast();
                    }
                }
                base.evaluate();
            }
        };
    }

    // Avoids closing the primary tab (and killing the renderer) in order to reset tab state
    // quickly, at the cost of thoroughness. This should be adequate for most tests.
    private void resetTabStateFast() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Close all but the first tab as these tests expect to start with a single
            // tab.
            while (TabModelUtils.closeTabByIndex(sActivity.getCurrentTabModel(), 1)) {
            }
        });
        mActivityTestRule.loadUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivity.getCurrentWebContents().getNavigationController().clearHistory();
        });
    }

    // Thoroughly resets tab state by closing all tabs before restoring the primary tab to
    // about:blank state.
    private void resetTabStateThorough() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivity.getTabModelSelector().closeAllTabs();
            sActivity.getTabCreator(false).launchUrl("about:blank", TabLaunchType.FROM_CHROME_UI);
        });
    }
}
