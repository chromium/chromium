// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.Observer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripHeightObserver;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceReadyOnceCallback;

/**
 * Owned and used by {@link TabStripTransitionCoordinator} to manage tab strip height transitions
 * that may be triggered by one of the following conditions: 1. Window resizing that warrants
 * showing / hiding the strip by updating its height maintained by the control container. 2. Entry
 * into / exit from desktop windowing mode that warrants the strip height to be updated to include /
 * exclude a reserved strip top padding.
 */
class HeightTransitionHandler {
    private static final String TAG = "DTCStripTransition";

    // Minimum width (in dp) of the screen for the tab strip to be shown.
    private static final int TRANSITION_THRESHOLD_DP = 412;

    private final ObserverList<TabStripHeightObserver> mTabStripHeightObservers =
            new ObserverList<>();
    private final CallbackController mCallbackController;
    private final Handler mHandler;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final ControlContainer mControlContainer;
    private final View mToolbarLayout;
    private final int mTabStripHeightFromResource;

    private OneshotSupplier<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier;

    /**
     * Current height of the tab strip represented by the space reserved on top of the toolbar
     * layout. Could be inconsistent with {@link #mTabStripVisible} during the transition.
     */
    private int mTabStripHeight;

    private int mTabStripTransitionThreshold;

    /**
     * The stable state whether the tab strip will be visible or not. This value will be changed at
     * the beginning of the transition.
     */
    private boolean mTabStripVisible;

    /** Tracks the last width seen for the tab strip. */
    private int mTabStripWidth;

    /** Tracks the additional top padding added to the tab strip. */
    private int mTopPadding;

    private boolean mIsDestroyed;

    private @Nullable BrowserControlsStateProvider.Observer mTransitionKickoffObserver;
    private @Nullable BrowserControlsStateProvider.Observer mTransitionFinishedObserver;

    private boolean mForceUpdateHeight;

    /**
     * Create the manager for transitions to show / hide the tab strip by updating the strip height.
     *
     * @param browserControlsVisibilityManager {@link BrowserControlsVisibilityManager} to observe
     *     browser controls height and animation state.
     * @param controlContainer The {@link ControlContainer} for the containing activity.
     * @param toolbarLayout {@link ToolbarLayout} for the current toolbar.
     * @param tabStripHeightFromResource The height of the tab strip defined in resource.
     * @param callbackController The {@link CallbackController} used by {@link
     *     TabStripTransitionCoordinator}.
     * @param handler The {@link Handler} used by {@link TabStripTransitionCoordinator}.
     * @param tabStripTransitionDelegateSupplier Supplier for the {@link
     *     TabStripTransitionDelegate}.
     */
    HeightTransitionHandler(
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            ControlContainer controlContainer,
            View toolbarLayout,
            int tabStripHeightFromResource,
            @NonNull CallbackController callbackController,
            @NonNull Handler handler,
            OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier) {
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mControlContainer = controlContainer;
        mToolbarLayout = toolbarLayout;
        mTabStripHeightFromResource = tabStripHeightFromResource;
        mCallbackController = callbackController;
        mHandler = handler;
        mTabStripTransitionDelegateSupplier = tabStripTransitionDelegateSupplier;

        mTabStripHeight = tabStripHeightFromResource;
        mTabStripVisible = mTabStripHeight > 0;
    }

    /** Remove observers and release reference to dependencies. */
    public void destroy() {
        mIsDestroyed = true;

        if (mTransitionKickoffObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mTransitionKickoffObserver);
            mTransitionKickoffObserver = null;
        }
        if (mTransitionFinishedObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mTransitionFinishedObserver);
            mTransitionFinishedObserver = null;
        }
        mTabStripHeightObservers.clear();
    }

    /** Return the current tab strip height. */
    int getTabStripHeight() {
        return mTabStripHeight;
    }

    /** Add observer for tab strip height change. */
    void addObserver(TabStripHeightObserver observer) {
        mTabStripHeightObservers.addObserver(observer);
    }

    /** Remove observer for tab strip height change. */
    void removeObserver(TabStripHeightObserver observer) {
        mTabStripHeightObservers.removeObserver(observer);
    }

    void updateTabStripTransitionThreshold(DisplayMetrics displayMetrics) {
        mTabStripTransitionThreshold =
                ViewUtils.dpToPx(displayMetrics, getScreenWidthThresholdDp());

        if (TabStripTransitionCoordinator.sHeightTransitionThresholdForTesting != null) {
            requestTransition();
        }
    }

    void setTabStripSize(int width, int topPadding) {
        mTabStripWidth = width;
        mTopPadding = topPadding;
    }

    void requestTransition() {
        mTabStripTransitionDelegateSupplier.runSyncOrOnAvailable(
                mCallbackController.makeCancelable(delegate -> maybeUpdateTabStripVisibility()));
    }

    private void maybeUpdateTabStripVisibility() {
        // Do not allow callback to pass through when object is destroyed.
        if (mIsDestroyed) return;

        // Invalid width / height will be ignored. This can happen when the mControlContainer is
        // created hidden after theme changes. See crbug.com/1511599.
        if (mTabStripWidth <= 0 || controlContainerView().getHeight() == 0) return;

        boolean showTabStrip = mTabStripWidth >= mTabStripTransitionThreshold;
        if (showTabStrip == mTabStripVisible && !mForceUpdateHeight) {
            // Do not transition if visibility does not change, unless we are changing the desktop
            // windowing mode, when we want to continue the transition to update the tab strip top
            // padding.
            return;
        }

        // Update the min size for the control container. This is needed one-layout-before browser
        // controls start changing its height, as it assumed a fixed size control container during
        // transition. See b/324178484.
        View toolbarHairline = controlContainerView().findViewById(R.id.toolbar_hairline);
        int maxHeight =
                calculateTabStripHeight()
                        + mToolbarLayout.getMeasuredHeight()
                        + toolbarHairline.getMeasuredHeight();
        controlContainerView().setMinimumHeight(maxHeight);

        // When transition kicked off by the BrowserControlsManager, the toolbar capture can be
        // stale e.g. still with the previous window width. Force invalidate the toolbar capture to
        // make sure the it's up-to-date with the latest Android view.
        var resourceAdapter = mControlContainer.getToolbarResourceAdapter();
        DynamicResourceReadyOnceCallback.onNext(
                resourceAdapter, (resource) -> setTabStripVisibility(showTabStrip));

        // Post the invalidate to make sure another layout pass is done. This is to make sure the
        // omnibox has the URL text updated to the final width of location bar after the toolbar
        // tablet button animations.
        // TODO(crbug.com/41493621): Trigger bitmap capture without mHandler#post.
        // TODO(crbug.com/41494086): Remove #invalidate after CaptureObservers respect a null
        // dirtyRect input.
        mHandler.post(
                mCallbackController.makeCancelable(
                        () -> {
                            resourceAdapter.invalidate(null);
                            resourceAdapter.triggerBitmapCapture();
                        }));
    }

    void setForceUpdateHeight(boolean forceUpdateHeight) {
        mForceUpdateHeight = forceUpdateHeight;
    }

    private View controlContainerView() {
        return mControlContainer.getView();
    }

    /** This may differ from |mTabStripHeight| when the transition is about to start. */
    private int calculateTabStripHeight() {
        return mTabStripHeightFromResource + mTopPadding;
    }

    /**
     * Set the new height for the tab strip. This on high level consists with 3 steps:
     *
     * <ul>
     *   <li>1. Use BrowserControlsSizer to change the browser control height. This will kick off
     *       the scene layer transition.
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
        if (mIsDestroyed) return;

        mTabStripVisible = show;
        // Use a non-zero height when |mForceUpdateHeight| is set.
        int newHeight = show || mForceUpdateHeight ? calculateTabStripHeight() : 0;

        // TODO(crbug.com/41484284): Maybe handle mid-progress pivots for browser controls.
        if (mTransitionFinishedObserver != null) {
            Log.w(
                    TAG,
                    "TransitionFinishedObserver is not cleared when new transition starts. This"
                            + " means previous transition was not finished properly.");
            notifyTransitionFinished(false);
            recordTabStripTransitionFinished(false);
        }

        // TODO(crbug.com/41481630): Request directly instead of using observer interface.
        for (var observer : mTabStripHeightObservers) {
            observer.onTransitionRequested(newHeight);
        }

        // If the browser control is performing an browser initiated animation, we should update the
        // view margins right away. This will make sure the toolbar stays in the same place with
        // changes in control container's Y translation.
        boolean javaAnimationInProgress = mBrowserControlsVisibilityManager.offsetOverridden();

        // For cases where transition is finished in sequence during #onTransitionRequested (e.g.
        // browser control's visibility is under constraint), we'll call updateTabStripHeightImpl
        // to update the margin for the views.
        boolean browserControlsHasConstraint =
                mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate().get()
                        != BrowserControlsState.BOTH;

        if (javaAnimationInProgress || browserControlsHasConstraint) {
            updateTabStripHeightImpl();
            return;
        }

        // For cc initiated transition, we'll defer the view updates until the first
        // #onControlsOffsetChanged is called. This prevents the toolbar margins from getting
        // updated too fast before the cc layer responds, in which case the Android views in the
        // browser control are still visible.
        if (mTransitionKickoffObserver != null) return;
        mTransitionKickoffObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        updateTabStripHeightImpl();
                    }

                    @Override
                    public void onAndroidControlsVisibilityChanged(int visibility) {
                        // Update the margin when browser control turns into invisible. This can
                        // happen before onControlsOffsetChanged.
                        updateTabStripHeightImpl();
                    }
                };
        mBrowserControlsVisibilityManager.addObserver(mTransitionKickoffObserver);
    }

    // TODO(crbug.com/40939440): Find a better place to set these top margins.
    private void updateTabStripHeightImpl() {
        // Remove the mBrowserControlsObserver, to make sure this method is called only once.
        if (mTransitionKickoffObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mTransitionKickoffObserver);
            mTransitionKickoffObserver = null;
        }

        // Change the height when we change the margin, to reflect the actual
        // tab strip height. Check the height to make sure this is only called once.
        // Use a non-zero height when |mForceUpdateHeight| is set.
        int height = mTabStripVisible || mForceUpdateHeight ? calculateTabStripHeight() : 0;
        if (mTabStripHeight == height) return;
        mTabStripHeight = height;

        // The top margin is the space left for the tab strip.
        updateTopMargin(mToolbarLayout, mTabStripHeight);

        // Change the toolbar hairline top margin.
        int topControlHeight = mTabStripHeight + mToolbarLayout.getHeight();
        View toolbarHairline = controlContainerView().findViewById(R.id.toolbar_hairline);
        updateTopMargin(toolbarHairline, topControlHeight);

        // Update the find toolbar and toolbar drop target views. Do not update find_toolbar_stub
        // since it is only used for phones.
        // TODO (crbug.com/1517059): Let FindToolbar itself decide how to set the top margin.
        updateViewStubTopMargin(R.id.find_toolbar_tablet_stub, R.id.find_toolbar, topControlHeight);
        updateViewStubTopMargin(
                R.id.target_view_stub, R.id.toolbar_drag_drop_target_view, mTabStripHeight);

        assert mTabStripTransitionDelegateSupplier.get() != null
                : "TabStripTransitionDelegate should be available.";
        mTabStripTransitionDelegateSupplier.get().onHeightChanged(mTabStripHeight);

        // If top control is already at steady state, notify right away.
        if (isTopControlAtSteadyState()) {
            notifyTransitionFinished(true);
            recordTabStripTransitionFinished(true);
            return;
        }
        // Otherwise, wait for the content offset to read steady state before notifying.
        assert mTransitionFinishedObserver == null;
        mTransitionFinishedObserver =
                new Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        if (isTopControlAtSteadyState()) {
                            notifyTransitionFinished(true);
                            recordTabStripTransitionFinished(true);
                        }
                    }
                };
        mBrowserControlsVisibilityManager.addObserver(mTransitionFinishedObserver);
    }

    private void updateViewStubTopMargin(int viewStubId, int inflatedViewId, int topMargin) {
        View view = controlContainerView().findViewById(inflatedViewId);
        if (view != null) {
            updateTopMargin(view, topMargin);
        } else {
            // View is not yet inflated.
            View viewStub = controlContainerView().findViewById(viewStubId);
            updateTopMargin(viewStub, topMargin);
        }
    }

    private static void updateTopMargin(View view, int topMargin) {
        ViewGroup.MarginLayoutParams hairlineParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        hairlineParams.topMargin = topMargin;
        view.setLayoutParams(hairlineParams);
    }

    private boolean isTopControlAtSteadyState() {
        // At steady state, the top control's height will match the content offset;
        boolean topControlsAtSteadyState =
                mBrowserControlsVisibilityManager.getContentOffset()
                        == mBrowserControlsVisibilityManager.getTopControlsHeight();

        // The browser controls transition might be interrupted (e.g. by user
        // scrolling). In such case, when browser controls can reach the
        // "end" state without being at the steady state. We can check if browser
        // controls is entirely off screen by user scroll and dispatch the notify
        // signal.
        boolean browserControlsInvisible =
                mBrowserControlsVisibilityManager.getContentOffset() == 0;

        // TODO(crbug.com/41484284): Dispatch the transition finished signal sooner
        //  when interruption is detected.
        return topControlsAtSteadyState || browserControlsInvisible;
    }

    private void notifyTransitionFinished(boolean measureControlContainer) {
        mBrowserControlsVisibilityManager.removeObserver(mTransitionFinishedObserver);
        // Reset internal state after transition ends.
        mForceUpdateHeight = false;
        mTransitionFinishedObserver = null;

        assert mTabStripTransitionDelegateSupplier.get() != null
                : "TabStripTransitionDelegate should be available.";
        mTabStripTransitionDelegateSupplier.get().onHeightTransitionFinished();

        if (measureControlContainer) remeasureControlContainer();
    }

    private void remeasureControlContainer() {
        // Remeasure the control container in the next layout pass if needed. The post is needed due
        // to the existence of ToolbarProgressBar adjusting its position based on ToolbarLayout's
        // layout pass. The control container needs to remeasure based on the new margin in order to
        // retain the up-to-date size with size reduction for the tab strip.

        // This is not done during the transition as it could cause visual glitches.
        mHandler.post(
                mCallbackController.makeCancelable(
                        () -> {
                            View toolbarHairline =
                                    controlContainerView().findViewById(R.id.toolbar_hairline);
                            controlContainerView()
                                    .setMinimumHeight(
                                            mToolbarLayout.getHeight()
                                                    + mTabStripHeight
                                                    + toolbarHairline.getHeight());
                            ViewUtils.requestLayout(
                                    controlContainerView(),
                                    "TabStripTransitionCoordinator.remeasureControlContainer");
                        }));
    }

    private void recordTabStripTransitionFinished(boolean finished) {
        RecordHistogram.recordBooleanHistogram(
                "Android.DynamicTopChrome.TabStripTransition.Finished", finished);
    }

    /** Get the min screen width (in dp) required for the tab strip to become visible. */
    private static int getScreenWidthThresholdDp() {
        if (TabStripTransitionCoordinator.sHeightTransitionThresholdForTesting != null) {
            return TabStripTransitionCoordinator.sHeightTransitionThresholdForTesting;
        }
        return TRANSITION_THRESHOLD_DP;
    }
}
