// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewParent;

import androidx.annotation.IdRes;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarView;

/** Utility methods for bottom bar tests. */
public class BottomBarTestUtils {
    /**
     * Whether the bottom bar is currently expected to be visible for the given activity based on
     * its state and configuration (e.g. not disabled on NTP).
     */
    public static boolean isBottomBarVisible(Activity activity) {
        if (!BottomBarConfigUtils.isBottomBarEnabled(activity)) {
            return false;
        }
        if (isRegularNtp(activity) && BottomBarConfigUtils.shouldDisableOnNtp()) {
            return false;
        }
        if (activity instanceof ChromeTabbedActivity tabbedActivity
                && !BottomBarConfigUtils.shouldShowOnGts()
                && ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabbedActivity.getLayoutManager() != null
                                        && tabbedActivity
                                                .getLayoutManager()
                                                .isLayoutVisible(LayoutType.HUB))) {
            return false;
        }
        return true;
    }

    /**
     * Finds the optional/adaptive toolbar button (like Share, New Tab, Voice).
     *
     * @param activity The activity containing the button.
     * @return The optional button view, or null if not found.
     */
    public static View findOptionalButton(Activity activity) {
        if (activity instanceof ChromeActivity chromeActivity) {
            ToolbarManager toolbarManager = chromeActivity.getToolbarManager();
            if (toolbarManager != null) {
                View button =
                        ThreadUtils.runOnUiThreadBlocking(
                                () ->
                                        toolbarManager
                                                .getToolbarLayoutForTesting()
                                                .getOptionalButtonViewForTesting());
                if (button != null) return button;
            }
        }

        return activity.findViewById(R.id.optional_toolbar_button);
    }

    /**
     * Finds a view in the Activity, preferring the bottom bar if the bottom bar is visible.
     *
     * @param activity The activity to search.
     * @param viewId The resource ID of the view to find.
     * @return The found view, or null.
     */
    public static <T extends View> T findViewById(Activity activity, @IdRes int viewId) {
        BottomBarView bottomBar = null;
        if (isBottomBarVisible(activity)) {
            bottomBar =
                    activity.findViewById(
                            org.chromium.chrome.browser.ui.bottombar.R.id.bottom_bar_container);
            if (bottomBar != null) {
                // The app menu button wrapper is not used in the bottom bar, only the button.
                @IdRes
                int bottomBarViewId =
                        viewId == R.id.menu_button_wrapper ? R.id.menu_button : viewId;

                final BottomBarView finalBottomBar = bottomBar;
                ThreadUtils.runOnUiThreadBlocking(() -> finalBottomBar.inflateAllStubsForTesting());
                T view = bottomBar.findViewById(bottomBarViewId);
                if (view != null) {
                    if (view.getParent() instanceof View parent) {
                        if (parent.getVisibility() == View.VISIBLE) {
                            return view;
                        }
                    } else {
                        return view;
                    }
                }
            }
        }

        T fallbackView = activity.findViewById(viewId);
        // fallbackView might find the exact same GONE bottom bar view we rejected above.
        // We return null instead to ensure tests correctly see the view is absent.
        if (fallbackView != null && bottomBar != null && isChildOf(fallbackView, bottomBar)) {
            return null;
        }
        return fallbackView;
    }

    /**
     * Finds a view in the view hierarchy, preferring the bottom bar if the bottom bar is visible.
     *
     * @param rootView The root view to search from.
     * @param viewId The resource ID of the view to find.
     * @return The found view, or null.
     */
    public static <T extends View> T findViewById(View rootView, @IdRes int viewId) {
        Context context = rootView.getContext();
        View bottomBar = null;
        Activity activity = ContextUtils.activityFromContext(context);
        if (activity != null && isBottomBarVisible(activity)) {
            bottomBar =
                    rootView.findViewById(
                            org.chromium.chrome.browser.ui.bottombar.R.id.bottom_bar_container);
            if (bottomBar instanceof BottomBarView bottomBarView) {
                @IdRes
                int bottomBarViewId =
                        viewId == R.id.menu_button_wrapper ? R.id.menu_button : viewId;
                ThreadUtils.runOnUiThreadBlocking(() -> bottomBarView.inflateAllStubsForTesting());
                T view = bottomBar.findViewById(bottomBarViewId);
                if (view != null) {
                    if (view.getParent() instanceof View parent) {
                        if (parent.getVisibility() == View.VISIBLE) {
                            return view;
                        }
                    } else {
                        return view;
                    }
                }
            }
        }

        T fallbackView = rootView.findViewById(viewId);
        if (fallbackView != null && bottomBar != null && isChildOf(fallbackView, bottomBar)) {
            // Fallback view might find the exact same GONE bottom bar view we rejected above.
            // We return null instead to ensure tests correctly see the view is absent.
            return null;
        }
        return fallbackView;
    }

    /** Returns whether the current tab for the given activity is a regular New Tab Page. */
    private static boolean isRegularNtp(Activity activity) {
        if (activity instanceof ChromeTabbedActivity chromeActivity) {
            return ThreadUtils.runOnUiThreadBlocking(
                    () -> BottomBarConfigUtils.isRegularNtp(chromeActivity.getActivityTab()));
        }
        return false;
    }

    private static boolean isChildOf(View child, View potentialParent) {
        if (child == potentialParent) return true;
        ViewParent parent = child.getParent();
        while (parent != null) {
            if (parent == potentialParent) return true;
            parent = parent.getParent();
        }
        return false;
    }
}
