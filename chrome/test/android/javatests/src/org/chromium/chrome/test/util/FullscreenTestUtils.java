// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.os.Build;
import android.view.View;
import android.view.WindowManager;

import org.hamcrest.Matchers;

import org.chromium.base.BuildInfo;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;

/** Static methods for use in tests that require toggling persistent fullscreen. */
public class FullscreenTestUtils {

    /**
     * Toggles persistent fullscreen for the tab and waits for the fullscreen flag to be set and the
     * tab to enter persistent fullscreen state.
     *
     * @param tab The {@link Tab} to toggle fullscreen on.
     * @param state Whether the tab should be set to fullscreen.
     * @param activity The {@link Activity} owning the tab.
     */
    public static void togglePersistentFullscreenAndAssert(
            final Tab tab, final boolean state, Activity activity) {
        togglePersistentFullscreenAndAssert(tab, state, activity, false, false);
    }

    /**
     * Toggles persistent fullscreen for the tab and waits for the fullscreen flag to be set and the
     * tab to enter persistent fullscreen state.
     *
     * @param tab The {@link Tab} to toggle fullscreen on.
     * @param state Whether the tab should be set to fullscreen.
     * @param activity The {@link Activity} owning the tab.
     * @param prefersNavigationBar Whether navigation bar should be shown when in fullscreen.
     * @param prefersStatusBar Whether status bar should be shown when in fullscreen.
     */
    public static void togglePersistentFullscreenAndAssert(
            final Tab tab,
            final boolean state,
            Activity activity,
            boolean prefersNavigationBar,
            boolean prefersStatusBar) {
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);
        FullscreenTestUtils.togglePersistentFullscreen(
                delegate, state, prefersNavigationBar, prefersStatusBar);
        // In order for the status bar to be displayed, the fullscreen flag must not be set.
        // If we are entering fullscreen, then we expect the fullscreen flag state to match
        // negated |prefersStatusBar|:
        FullscreenTestUtils.waitForFullscreenFlag(tab, state && !prefersStatusBar, activity);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, state);
    }

    /**
     * Toggles persistent fullscreen for the tab.
     *
     * @param delegate The {@link TabWebContentsDelegateAndroid} for the tab.
     * @param state Whether the tab should be set to fullscreen.
     */
    public static void togglePersistentFullscreen(
            final TabWebContentsDelegateAndroid delegate, final boolean state) {
        togglePersistentFullscreen(delegate, state, false, false);
    }

    /**
     * Toggles persistent fullscreen for the tab.
     *
     * @param delegate The {@link TabWebContentsDelegateAndroid} for the tab.
     * @param state Whether the tab should be set to fullscreen.
     * @param prefersNavigationBar Whether navigation bar should be shown when in fullscreen.
     * @param prefersStatusBar Whether status bar should be shown when in fullscreen.
     */
    public static void togglePersistentFullscreen(
            final TabWebContentsDelegateAndroid delegate,
            final boolean state,
            boolean prefersNavigationBar,
            boolean prefersStatusBar) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (state) {
                        delegate.enterFullscreenModeForTab(prefersNavigationBar, prefersStatusBar);
                    } else {
                        delegate.exitFullscreenModeForTab();
                    }
                });
    }

    /**
     * Waits for the fullscreen flag to be set on the specified {@link Tab}.
     *
     * @param tab The {@link Tab} that is expected to have the flag set.
     * @param state Whether the tab should be to fullscreen.
     * @param activity The {@link Activity} owning the tab.
     */
    public static void waitForFullscreenFlag(
            final Tab tab, final boolean state, final Activity activity) {
        CriteriaHelper.pollUiThread(
                () -> isFullscreenFlagSet(tab, state, activity),
                6000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    public static void waitForHideNavigationFlag(
            final Tab tab, final boolean state, final Activity activity) {
        CriteriaHelper.pollUiThread(
                () -> isHideNavigationFlagSet(tab, state, activity),
                6000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits for the specified {@link Tab} to enter fullscreen. mode
     *
     * @param delegate The {@link TabWebContentsDelegateAndroid} for the tab.
     * @param state Whether the tab should be set to fullscreen.
     */
    public static void waitForPersistentFullscreen(
            final TabWebContentsDelegateAndroid delegate, boolean state) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(delegate.isFullscreenForTabOrPending(), Matchers.is(state));
                });
    }

    private static boolean isFlagSet(int flags, int flag) {
        return (flags & flag) == flag;
    }

    private static boolean isFullscreenFlagSet(
            final Tab tab, final boolean state, Activity activity) {
        if (BuildInfo.getInstance().isAutomotive) {
            return true;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            View view = tab.getContentView();
            int visibility = view.getSystemUiVisibility();

            // SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN should only be used during the transition between
            // fullscreen states, so it should always be cleared when fullscreen transitions are
            // completed.
            return (!isFlagSet(visibility, View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN))
                    && (isFlagSet(visibility, View.SYSTEM_UI_FLAG_FULLSCREEN) == state);
        } else {
            WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
            return isFlagSet(attributes.flags, WindowManager.LayoutParams.FLAG_FULLSCREEN) == state;
        }
    }

    private static boolean isHideNavigationFlagSet(
            final Tab tab, final boolean state, Activity activity) {
        View view = tab.getContentView();
        int visibility = view.getSystemUiVisibility();

        return isFlagSet(visibility, View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == state;
    }
}
