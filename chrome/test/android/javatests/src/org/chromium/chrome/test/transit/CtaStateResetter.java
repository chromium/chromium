// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ChromePopupWindow;

import java.util.Collections;

/**
 * Stateless helper that scrubs a live {@link ChromeTabbedActivity} back to a single blank tab in
 * the browsing layout between batched tests.
 */
@NullMarked
final class CtaStateResetter {
    private static final String TAG = "Transit";

    private CtaStateResetter() {}

    static void resetToSingleBlankTab(
            ChromeTabbedActivityTestRule activityTestRule, boolean clearAllTabState) {
        ChromeTabbedActivity activity = activityTestRule.getActivity();
        // Must run before the reset, which waits on tab interactability and the BROWSING layout.
        ThreadUtils.runOnUiThreadBlocking(() -> resetDialogsAndKeyboardOnUiThread(activity));
        ChromeActivityTestRule.ensureActivityFocused(activity);
        if (!clearAllTabState && activityTestRule.tabsCount(/* incognito= */ false) > 0) {
            resetTabStateFast(activityTestRule);
        } else {
            resetTabStateThorough(activity);
        }
    }

    // Avoids closing the primary tab (and killing the renderer) in order to reset tab state
    // quickly, at the cost of thoroughness. This should be adequate for most tests.
    static void resetTabStateFast(ChromeTabbedActivityTestRule activityTestRule) {
        ChromeTabbedActivity activity = activityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(() -> resetTabStateFastOnUiThread(activity));
        activityTestRule.loadUrl("about:blank");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assumeNonNull(activity.getCurrentWebContents())
                                .getNavigationController()
                                .clearHistory());
    }

    // Thoroughly resets tab state by closing all tabs before restoring the primary tab to
    // about:blank state.
    private static void resetTabStateThorough(ChromeTabbedActivity activity) {
        Tab createdTab =
                ThreadUtils.runOnUiThreadBlocking(() -> resetTabStateThoroughOnUiThread(activity));
        ChromeTabUtils.waitForTabPageLoaded(createdTab, "about:blank");
        LayoutTestUtils.waitForLayout(activity.getLayoutManager(), LayoutType.BROWSING);
    }

    private static void resetTabStateFastOnUiThread(ChromeTabbedActivity activity) {
        ThreadUtils.assertOnUiThread();
        IncognitoTabHostUtils.closeAllIncognitoTabs();
        // Close all but the first regular tab as these tests expect to start with a single tab.
        TabModel regularTabModel = activity.getTabModelSelector().getModel(/* incognito= */ false);
        closeAllTabsExcept(regularTabModel, regularTabModel.getTabAt(0));

        Tab activityTab = activity.getActivityTab();
        if (activityTab != null) {
            ungroupAndUnpinTabOnUiThread(regularTabModel, activityTab);
        }
        ensureBrowsingLayoutOnUiThread(activity.getLayoutManager());
    }

    private static void ungroupAndUnpinTabOnUiThread(TabModel regularTabModel, Tab activityTab) {
        ThreadUtils.assertOnUiThread();
        if (regularTabModel.isTabInTabGroup(activityTab)) {
            regularTabModel
                    .getTabUngrouper()
                    .ungroupTabs(
                            Collections.singletonList(activityTab),
                            /* trailing= */ false,
                            /* allowDialog= */ false);
            // If the group is a collaboration it is possible a tab got added to preserve the group.
            // If this happens delete the tab forcibly.
            closeAllTabsExcept(regularTabModel, activityTab);
        }
        if (activityTab.getIsPinned()) {
            regularTabModel.unpinTab(activityTab.getId());
        }
    }

    private static Tab resetTabStateThoroughOnUiThread(ChromeTabbedActivity activity) {
        ThreadUtils.assertOnUiThread();
        // We have to avoid closing all tabs and triggering CTA's self-finish logic when all tabs
        // are closed.
        Tab newTab =
                assertNonNull(
                        activity.getTabCreator(/* incognito= */ false)
                                .launchUrl("about:blank", TabLaunchType.FROM_CHROME_UI));
        IncognitoTabHostUtils.closeAllIncognitoTabs();

        TabModel regularTabModel = activity.getTabModelSelector().getModel(/* incognito= */ false);
        closeAllTabsExcept(regularTabModel, newTab);
        // Closing the last tab in a batched test initiates an asynchronous transition to
        // LayoutType.HUB that is not cancelled on tablets when a new tab is launched above. Ensure
        // LayoutType.BROWSING is active before the next test starts.
        ensureBrowsingLayoutOnUiThread(activity.getLayoutManager());
        return newTab;
    }

    private static void resetDialogsAndKeyboardOnUiThread(ChromeTabbedActivity activity) {
        ThreadUtils.assertOnUiThread();
        int dismissedPopups = ChromePopupWindow.dismissAllPopupsForTesting();
        if (dismissedPopups > 0) {
            Log.w(
                    TAG,
                    "Dismissed %d lingering ChromePopupWindow(s) during batched state reset.",
                    dismissedPopups);
        }
        if (activity.getWindow() != null && activity.getWindow().getDecorView() != null) {
            KeyboardUtils.hideAndroidSoftKeyboard(activity.getWindow().getDecorView());
        }
        ModalDialogManager modalDialogManager = activity.getModalDialogManager();
        if (modalDialogManager != null) {
            modalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
        }
    }

    private static void closeAllTabsExcept(TabModel tabModel, @Nullable Tab tabToKeep) {
        ThreadUtils.assertOnUiThread();
        for (int i = tabModel.getCount() - 1; i >= 0; i--) {
            Tab tab = tabModel.getTabAt(i);
            if (tab != null && tab != tabToKeep) {
                tabModel.getTabRemover()
                        .forceCloseTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
            }
        }
    }

    private static void ensureBrowsingLayoutOnUiThread(LayoutManagerChrome layoutManager) {
        ThreadUtils.assertOnUiThread();
        if (!layoutManager.isLayoutVisible(LayoutType.BROWSING)
                || layoutManager.isLayoutStartingToHide(LayoutType.BROWSING)) {
            layoutManager.showLayout(LayoutType.BROWSING, /* animate= */ false);
        }
    }
}
