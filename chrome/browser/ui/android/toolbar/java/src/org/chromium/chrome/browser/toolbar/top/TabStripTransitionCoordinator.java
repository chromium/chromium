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
    public interface TabStripHeightObserver {
        /** Notified when the tab strip height is about to change. */
        void onTabStripHeightChanged(int newHeight);
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
        mCallbackController.destroy();
        mTabStripHeightObservers.clear();
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration configuration) {
        DisplayMetrics displayMetrics = mControlContainer.getResources().getDisplayMetrics();
        int width = displayMetrics.widthPixels;
        boolean showTabStrip =
                width >= ViewUtils.dpToPx(displayMetrics, getScreenWidthThresholdDp());

        if (showTabStrip == mTabStripVisible) return;

        // Kick off tab strip transition once tab strip visibility is confirmed to be changed. Do
        // not change the mTabStripVisible until the transition actually started.
        if (mLastTransitionTask != null) {
            mHandler.removeCallbacks(mLastTransitionTask);
        }
        mLastTransitionTask =
                mCallbackController.makeCancelable(
                        () -> maybeUpdateTabStripVisibility(showTabStrip));
        mHandler.postDelayed(mLastTransitionTask, TRANSITION_DELAY_MS);
    }

    @Override
    public void onLowMemory() {}

    private void maybeUpdateTabStripVisibility(boolean showTabStrip) {
        if (showTabStrip == mTabStripVisible) return;
        mTabStripVisible = showTabStrip;

        // If control container is already stable, or the Android view is invisible, perform the
        // layout directly; otherwise, wait for one layout pass so the toolbar is redrawn at the
        // reduced width before capturing a new bitmap for the C++ animation.
        if (!mControlContainer.isInLayout()
                || mBrowserControlsVisibilityManager.getAndroidControlsVisibility()
                        != View.VISIBLE) {
            setTabStripVisibility(showTabStrip);
            return;
        }

        // If an layout is already scheduled for a different visibility, cancel it.
        if (mOnLayoutChangedListener != null) {
            mControlContainer.removeOnLayoutChangeListener(mOnLayoutChangedListener);
        }

        // Wait for one layout pass so the toolbar is redrawn at the reduced width before capturing
        // a new bitmap for the C++ animation.
        mOnLayoutChangedListener =
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    mControlContainer.removeOnLayoutChangeListener(mOnLayoutChangedListener);
                    mOnLayoutChangedListener = null;

                    setTabStripVisibility(showTabStrip);
                };
        mControlContainer.addOnLayoutChangeListener(mOnLayoutChangedListener);
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
        mTabStripHeight = show ? mTabStripHeightFromResource : 0;

        // Notify tab strip height is changed, and get ready for the browser controls updates.
        for (var observer : mTabStripHeightObservers) {
            observer.onTabStripHeightChanged(mTabStripHeight);
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
