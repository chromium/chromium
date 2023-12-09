// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** Subclass used to manage tab strip visibility and height presents. */
public class TabStripTransitionCoordinator implements ComponentCallbacks {
    // Delay to kickoff the transition to avoid frame drops while application is too busy when the
    // configuration changed.
    private static final int TRANSITION_DELAY_MS = 200;

    /** Observes height of tab strip that could change during run time. */
    // TODO(crbug.com/1509013): Rework the observer interface.
    public interface TabStripHeightObserver {
        /**
         * Called when the tab strip requests an update when control container changes its width.
         *
         * @param newHeight The expected height tab strip will be changed into.
         */
        default void onHeightTransitionRequested(int newHeight) {}

        /**
         * Called when the tab strip height changed. This height will match the space on top of the
         * toolbar reserved for the tab strip.
         *
         * @param newHeight The height same as {@link #getTabStripHeight()}.
         */
        default void onHeightChanged(int newHeight) {}
    }

    private static Integer sMinScreenWidthForTesting;

    private final ObserverList<TabStripHeightObserver> mTabStripHeightObservers =
            new ObserverList<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final Handler mHandler = new Handler();
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final View mControlContainer;
    private final View mToolbarLayout;
    private final int mTabStripHeightFromResource;

    private int mTabStripHeight;
    private int mTabStripTransitionThreshold;
    private boolean mTabStripVisible;
    private @Nullable BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private @Nullable OnLayoutChangeListener mOnLayoutChangedListener;
    private @Nullable Runnable mLastTransitionTask;

    /**
     * Create the coordinator managing transitions for when showing / hiding the tab strip.
     *
     * @param browserControlsVisibilityManager {@link BrowserControlsVisibilityManager} to observe
     *     browser controls height and animation state.
     * @param controlContainer The {@link ToolbarControlContainer} for the containing activity.
     * @param toolbarLayout {@link ToolbarLayout} for the current toolbar.
     */
    TabStripTransitionCoordinator(
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            View controlContainer,
            View toolbarLayout,
            int tabStripHeightFromResource) {
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mControlContainer = controlContainer;
        mToolbarLayout = toolbarLayout;
        mTabStripHeightFromResource = tabStripHeightFromResource;

        mTabStripHeight = tabStripHeightFromResource;
        mTabStripVisible = mTabStripHeight > 0;

        mOnLayoutChangedListener =
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    int windowWidth = Math.abs(right - left);
                    maybeUpdateTabStripVisibility(windowWidth);
                };
        mControlContainer.addOnLayoutChangeListener(mOnLayoutChangedListener);

        updateTabStripTransitionThreshold();
        maybeUpdateTabStripVisibility(mControlContainer.getWidth());
    }

    /** Return the current tab strip height. */
    public int getTabStripHeight() {
        return mTabStripHeight;
    }

    /** Add observer for tab strip height change. */
    public void addObserver(TabStripHeightObserver observer) {
        mTabStripHeightObservers.addObserver(observer);
    }

    /** Remove observer for tab strip height change. */
    public void removeObserver(TabStripHeightObserver observer) {
        mTabStripHeightObservers.removeObserver(observer);
    }

    /** Remove observers and release reference to dependencies. */
    public void destroy() {
        if (mBrowserControlsObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
            mBrowserControlsObserver = null;
        }
        if (mOnLayoutChangedListener != null) {
            mControlContainer.removeOnLayoutChangeListener(mOnLayoutChangedListener);
            mOnLayoutChangedListener = null;
        }
        mCallbackController.destroy();
        mTabStripHeightObservers.clear();
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration configuration) {
        updateTabStripTransitionThreshold();
    }

    @Override
    public void onLowMemory() {}

    private void updateTabStripTransitionThreshold() {
        DisplayMetrics displayMetrics = mControlContainer.getResources().getDisplayMetrics();
        mTabStripTransitionThreshold =
                ViewUtils.dpToPx(displayMetrics, getScreenWidthThresholdDp());

        if (sMinScreenWidthForTesting != null) {
            maybeUpdateTabStripVisibility(displayMetrics.widthPixels);
        }
    }

    private void maybeUpdateTabStripVisibility(int windowWidth) {
        boolean showTabStrip = windowWidth >= mTabStripTransitionThreshold;
        if (showTabStrip == mTabStripVisible) return;

        // Kick off tab strip transition once tab strip visibility is confirmed to be
        // changed. Do not change the mTabStripVisible until the transition actually
        // started.
        if (mLastTransitionTask != null) {
            mHandler.removeCallbacks(mLastTransitionTask);
        }
        mLastTransitionTask =
                mCallbackController.makeCancelable(() -> setTabStripVisibility(showTabStrip));
        mHandler.postDelayed(mLastTransitionTask, TRANSITION_DELAY_MS);
    }

    /**
     * Set the new height for the tab strip. This on high level consists with 3 steps:
     *
     * <ul>
     *   <li>1. Use {@link BrowserControlsSizer} to change the browser control height. This will
     *       kick off the scene layer transition.
     *   <li>2. Add / remove margins from the toolbar and toolbar hairline based on tab strip's new
     *       height.
     *   <li>3. Notify the tab strip scene layer for the new height. This will in turn notify
     *       StripLayoutHelperManager to mark a yOffset that tabStrip will render off-screen. Note
     *       that we cannot simply mark the tab strip scene layer hidden, since it's still required
     *       to be visible during the transition.
     * </ul>
     *
     * @param show Whether the tab strip should be shown.
     */
    private void setTabStripVisibility(boolean show) {
        mTabStripVisible = show;
        int newHeight = show ? mTabStripHeightFromResource : 0;

        // TODO(crbug.com/1509013): Request directly instead of using observer interface.
        for (var observer : mTabStripHeightObservers) {
            observer.onHeightTransitionRequested(newHeight);
        }

        if (mBrowserControlsObserver != null) return;
        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate) {
                        updateTabStripHeightImpl();
                    }

                    @Override
                    public void onAndroidControlsVisibilityChanged(int visibility) {
                        // Update the margin when browser control turns into invisible. This can
                        // happen before onControlsOffsetChanged.
                        updateTabStripHeightImpl();
                    }

                    @Override
                    public void onTopControlsHeightChanged(
                            int topControlsHeight, int topControlsMinHeight) {
                        // If the browser control is performing an browser initiated animation,
                        // we should update the view margins right away. This will make sure the
                        // toolbar stays in the same place with changes in control container's Y
                        // translation.
                        //
                        // For cc initiated transition, we'll defer the view updates until the first
                        // #onControlsOffsetChanged is called. This avoid the toolbar margins gets
                        // updated too fast before the cc layer respond, in which the Android views
                        // in the browser control are still visible.
                        if (mBrowserControlsVisibilityManager.offsetOverridden()) {
                            updateTabStripHeightImpl();
                        }
                    }
                };
        mBrowserControlsVisibilityManager.addObserver(mBrowserControlsObserver);
    }

    // TODO(crbug.com/1498252): Find a better place to set these top margins.
    private void updateTabStripHeightImpl() {
        // Remove the mBrowserControlsObserver, to make sure this method is called only once.
        if (mBrowserControlsObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
            mBrowserControlsObserver = null;
        }

        // Change the height when we change the margin, to reflect the actual
        // tab strip height.
        mTabStripHeight = mTabStripVisible ? mTabStripHeightFromResource : 0;

        // The top margin is the space left for the tab strip.
        updateTopMargin(mToolbarLayout, mTabStripHeight);

        // Change the toolbar hairline top margin.
        int topControlHeight = mTabStripHeight + mToolbarLayout.getHeight();
        View toolbarHairline = mControlContainer.findViewById(R.id.toolbar_hairline);
        updateTopMargin(toolbarHairline, topControlHeight);

        // optionally, update the find toolbar.
        View findToolbar = mControlContainer.findViewById(R.id.find_toolbar);
        if (findToolbar != null) {
            updateTopMargin(findToolbar, mTabStripHeight);
        } else {
            View findToolbarStub = mControlContainer.findViewById(R.id.find_toolbar_stub);
            updateTopMargin(findToolbarStub, mTabStripHeight);
        }

        for (var observer : mTabStripHeightObservers) {
            observer.onHeightChanged(mTabStripHeight);
        }
    }

    private static void updateTopMargin(View view, int topMargin) {
        ViewGroup.MarginLayoutParams hairlineParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        hairlineParams.topMargin = topMargin;
        view.setLayoutParams(hairlineParams);
    }

    /** Get the min screen width required in DP for the tab strip to become visible. */
    private static int getScreenWidthThresholdDp() {
        return sMinScreenWidthForTesting != null
                ? sMinScreenWidthForTesting
                : DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
    }

    /**
     * Set the test override that the min screen size for the tab strip to become visible.
     *
     * @param screenWidthForTesting Screen size in required for tab strip to become visible.
     */
    public static void setMinScreenWidthForTesting(int screenWidthForTesting) {
        sMinScreenWidthForTesting = screenWidthForTesting;
        ResettersForTesting.register(() -> sMinScreenWidthForTesting = null);
    }
}
