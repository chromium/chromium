// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.batch;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;

/**
 * To be used by batched tests that would like to reset to a single blank tab open in
 * ChromeTabbedActivity between each test, without restarting the Activity.
 *
 * <p>State is stored statically, and so the Activity may be reused across multiple test suites
 * within the same {@link Batch}.
 */
public class BlankCTATabInitialStateRule implements TestRule {
    private static ChromeTabbedActivity sActivity;

    private final ChromeTabbedActivityTestRule mActivityTestRule;
    private final boolean mClearAllTabState;

    /**
     * @param activityTestRule The ActivityTestRule to be reset to initial state between each test.
     * @param clearAllTabState More thoroughly clears all tab state, at the cost of restarting the
     *     renderer process between each test.
     */
    public BlankCTATabInitialStateRule(
            ChromeTabbedActivityTestRule activityTestRule, boolean clearAllTabState) {
        super();
        mActivityTestRule = activityTestRule;
        mClearAllTabState = clearAllTabState;
        mActivityTestRule.setFinishActivity(false);
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                if (sActivity == null) {
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                FirstRunStatus.setFirstRunFlowComplete(true);
                            });
                    if (mActivityTestRule.getActivity() == null) {
                        mActivityTestRule.startMainActivityOnBlankPage();
                    }
                    sActivity = mActivityTestRule.getActivity();

                    // Previous tests may have left tabs open and finished the Activity.
                    if (regularTabCount() > 1) resetTabStateFast();
                } else {
                    mActivityTestRule.setActivity(sActivity);
                    if (shouldPerformFastReset()) {
                        resetTabStateFast();
                    } else {
                        resetTabStateThorough();
                    }
                }
                try {
                    base.evaluate();
                } finally {
                    // If the activity was relaunched during the test, update the reference to use
                    // the most up to date Activity.
                    sActivity = mActivityTestRule.getActivity();
                    if (sActivity.isActivityFinishingOrDestroyed()) {
                        sActivity = null;
                    }
                }
            }
        };
    }

    private int regularTabCount() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return sActivity.getTabModelSelector().getModel(false).getCount();
                });
    }

    private boolean shouldPerformFastReset() {
        if (mClearAllTabState) return false;
        return regularTabCount() > 0;
    }

    // Avoids closing the primary tab (and killing the renderer) in order to reset tab state
    // quickly, at the cost of thoroughness. This should be adequate for most tests.
    private void resetTabStateFast() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IncognitoTabHostUtils.closeAllIncognitoTabs();
                    // Close all but the first regular tab as these tests expect to start with a
                    // single tab.
                    TabModel regularTabModel =
                            sActivity.getTabModelSelector().getModel(/* incognito= */ false);
                    while (TabModelUtils.closeTabByIndex(regularTabModel, 1)) {}
                });
        mActivityTestRule.loadUrl("about:blank");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.getCurrentWebContents().getNavigationController().clearHistory();
                    TabModelFilter filter =
                            sActivity
                                    .getTabModelSelector()
                                    .getTabModelFilterProvider()
                                    .getTabModelFilter(/* incognito= */ false);
                    Tab activityTab = sActivity.getActivityTab();
                    if (filter.isTabInTabGroup(activityTab)) {
                        ((TabGroupModelFilter) filter)
                                .moveTabOutOfGroupInDirection(
                                        activityTab.getId(), /* trailing= */ false);
                    }
                });
    }

    // Thoroughly resets tab state by closing all tabs before restoring the primary tab to
    // about:blank state.
    private void resetTabStateThorough() {
        Tab createdTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            // We have to avoid closing all tabs and triggering CTA's self-finish
                            // logic when all tabs are closed.
                            Tab newTab =
                                    sActivity
                                            .getTabCreator(false)
                                            .launchUrl("about:blank", TabLaunchType.FROM_CHROME_UI);
                            IncognitoTabHostUtils.closeAllIncognitoTabs();

                            TabModel regularTabModel =
                                    sActivity
                                            .getTabModelSelector()
                                            .getModel(/* incognito= */ false);
                            for (int i = regularTabModel.getCount() - 1; i >= 0; i--) {
                                Tab tab = regularTabModel.getTabAt(i);
                                if (tab != newTab) {
                                    regularTabModel.closeTabs(
                                            TabClosureParams.closeTab(tab)
                                                    .allowUndo(false)
                                                    .build());
                                }
                            }
                            return newTab;
                        });
        ChromeTabUtils.waitForTabPageLoaded(createdTab, "about:blank");
    }
}
