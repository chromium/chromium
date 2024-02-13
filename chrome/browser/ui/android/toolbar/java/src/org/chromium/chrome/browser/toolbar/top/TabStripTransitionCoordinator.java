// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.Observer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceReadyOnceCallback;
import org.chromium.ui.util.TokenHolder;

/** Subclass used to manage tab strip visibility and height presents. */
public class TabStripTransitionCoordinator implements ComponentCallbacks {
    private static final String TAG = "DTCStripTransition";

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
        default void onTransitionRequested(int newHeight) {}

        /**
         * Called when the tab strip height changed. This height will match the space on top of the
         * toolbar reserved for the tab strip.
         *
         * @param newHeight The height same as {@link #getTabStripHeight()}.
         */
        default void onHeightChanged(int newHeight) {}

        /** Notify when the tab strip transition is completed by the browser controls. */
        default void onTransitionFinished() {}
    }

    private static Integer sMinScreenWidthForTesting;

    private final ObserverList<TabStripHeightObserver> mTabStripHeightObservers =
            new ObserverList<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final Handler mHandler;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final ControlContainer mControlContainer;
    private final View mToolbarLayout;
    private final int mTabStripHeightFromResource;
    private final TabObscuringHandler mTabObscuringHandler;
    private final TokenHolder mDeferTransitionTokenHolder;

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

    /**
     * Internal state used to block the transition until the TRANSITION_DELAY_MS after the last
     * #onLayout pass.
     */
    private int mOnLayoutToken = TokenHolder.INVALID_TOKEN;

    /** Token used to block the transition when URL bar has focus. */
    private int mUrlBarFocusToken = TokenHolder.INVALID_TOKEN;

    private int mTabObscurToken = TokenHolder.INVALID_TOKEN;

    /** Tracks the last width seen for the mControlContainer. */
    private int mControlContainerLayoutWidth;

    private OnLayoutChangeListener mOnLayoutChangedListener;
    private TabObscuringHandler.Observer mTabObscuringHandlerObserver;
    private @Nullable Runnable mLayoutTransitionTask;

    private @Nullable BrowserControlsStateProvider.Observer mTransitionKickoffObserver;
    private @Nullable BrowserControlsStateProvider.Observer mTransitionFinishedObserver;

    /**
     * Create the coordinator managing transitions for when showing / hiding the tab strip.
     *
     * @param browserControlsVisibilityManager {@link BrowserControlsVisibilityManager} to observe
     *     browser controls height and animation state.
     * @param controlContainer The {@link ControlContainer} for the containing activity.
     * @param toolbarLayout {@link ToolbarLayout} for the current toolbar.
     * @param tabStripHeightFromResource The height of the tab strip defined in resource.
     */
    TabStripTransitionCoordinator(
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            ControlContainer controlContainer,
            View toolbarLayout,
            int tabStripHeightFromResource,
            TabObscuringHandler tabObscuringHandler) {
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mControlContainer = controlContainer;
        mToolbarLayout = toolbarLayout;
        mTabStripHeightFromResource = tabStripHeightFromResource;
        mHandler = new Handler(Looper.getMainLooper());

        mTabStripHeight = tabStripHeightFromResource;
        mTabStripVisible = mTabStripHeight > 0;

        mOnLayoutChangedListener =
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    int windowWidth = Math.abs(right - left);
                    onLayoutWidthChanged(windowWidth);
                };
        controlContainerView().addOnLayoutChangeListener(mOnLayoutChangedListener);
        mDeferTransitionTokenHolder = new TokenHolder(this::onTokenUpdate);

        mTabObscuringHandler = tabObscuringHandler;
        mTabObscuringHandlerObserver =
                (obscureTabContent, obscureToolbar) -> {
                    // Do not block transition if the toolbar is also obscured.
                    if (obscureToolbar) return;

                    if (obscureTabContent) {
                        int token = requestDeferTabStripTransitionToken();
                        if (mTabObscurToken != TokenHolder.INVALID_TOKEN) {
                            releaseTabStripToken(mTabObscurToken);
                        }
                        mTabObscurToken = token;
                    } else {
                        releaseTabStripToken(mTabObscurToken);
                        mTabObscurToken = TokenHolder.INVALID_TOKEN;
                    }
                };
        mTabObscuringHandler.addObserver(mTabObscuringHandlerObserver);

        updateTabStripTransitionThreshold();
        onLayoutWidthChanged(controlContainerView().getWidth());
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

    /** Request the token to defer the tab strip transition to a later time. */
    public int requestDeferTabStripTransitionToken() {
        return mDeferTransitionTokenHolder.acquireToken();
    }

    /**
     * Release the token acquired from {@link #requestDeferTabStripTransitionToken()} so tab strip
     * can transition based on its current sizes.
     */
    public void releaseTabStripToken(int token) {
        mDeferTransitionTokenHolder.releaseToken(token);
    }

    /** Remove observers and release reference to dependencies. */
    public void destroy() {
        if (mTransitionKickoffObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mTransitionKickoffObserver);
            mTransitionKickoffObserver = null;
        }
        if (mTransitionFinishedObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mTransitionFinishedObserver);
            mTransitionFinishedObserver = null;
        }
        if (mOnLayoutChangedListener != null) {
            controlContainerView().removeOnLayoutChangeListener(mOnLayoutChangedListener);
            mOnLayoutChangedListener = null;
        }
        if (mTabObscuringHandlerObserver != null) {
            mTabObscuringHandler.removeObserver(mTabObscuringHandlerObserver);
            mTabObscuringHandlerObserver = null;
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

    /**
     * Called when URL bar gains / lost focus. When gaining focus, block the tab strip transition.
     */
    // TODO(crbug.com/1519696): Remove this APIs - location bar is also using TabObscuringHandler.
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            int token = requestDeferTabStripTransitionToken();
            if (mUrlBarFocusToken != TokenHolder.INVALID_TOKEN) {
                releaseTabStripToken(mUrlBarFocusToken);
            }
            mUrlBarFocusToken = token;
        }
    }

    /** Called when URL bar focus animation finished. Release the token for tab strip transition. */
    public void onUrlAnimationFinished(boolean hasFocus) {
        if (!hasFocus) {
            releaseTabStripToken(mUrlBarFocusToken);
            mUrlBarFocusToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void onTokenUpdate() {
        maybeUpdateTabStripVisibility(controlContainerView().getWidth());
    }

    private View controlContainerView() {
        return mControlContainer.getView();
    }

    private void updateTabStripTransitionThreshold() {
        DisplayMetrics displayMetrics = controlContainerView().getResources().getDisplayMetrics();
        mTabStripTransitionThreshold =
                ViewUtils.dpToPx(displayMetrics, getScreenWidthThresholdDp());

        if (sMinScreenWidthForTesting != null) {
            onTokenUpdate();
        }
    }

    /**
     * Always wait for a short delay after the last #onLayout pass for the control container to make
     * sure the UI is in a stable state.
     *
     * @param newWidth The current width of control container.
     */
    private void onLayoutWidthChanged(int newWidth) {
        if (newWidth == mControlContainerLayoutWidth) return;
        mControlContainerLayoutWidth = newWidth;

        // Kick off tab strip transition once tab strip visibility is confirmed to be
        // changed. Do not change the mTabStripVisible until the transition actually
        // started.
        if (mLayoutTransitionTask != null) {
            mHandler.removeCallbacks(mLayoutTransitionTask);
        }
        int oldToken = mOnLayoutToken;
        mOnLayoutToken = mDeferTransitionTokenHolder.acquireToken();
        mDeferTransitionTokenHolder.releaseToken(oldToken);
        mLayoutTransitionTask =
                mCallbackController.makeCancelable(
                        () -> mDeferTransitionTokenHolder.releaseToken(mOnLayoutToken));
        mHandler.postDelayed(mLayoutTransitionTask, TRANSITION_DELAY_MS);
    }

    private void maybeUpdateTabStripVisibility(int tabStripWidth) {
        // Block new request for transitions as long as there's any token left. Once the token
        // clears out, #onTokenUpdated will route into this method again.
        if (mDeferTransitionTokenHolder.hasTokens()) return;

        // Invalid width will be ignored. This can happen when the mControlContainer is created
        // hidden after theme changes. See crbug.com/1511599.
        if (tabStripWidth <= 0) return;

        boolean showTabStrip = tabStripWidth >= mTabStripTransitionThreshold;
        if (showTabStrip == mTabStripVisible) return;

        // Update the min size for the control container. This is needed one-layout-before browser
        // controls start changing its height, as it assumed a fixed size control container during
        // transition. See b/324178484.
        int maxHeight = mTabStripHeightFromResource + mToolbarLayout.getMeasuredHeight();
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
        // TODO(crbug.com/1520644): Trigger bitmap capture without mHandler#post.
        // TODO(crbug.com/1521114): Remove #invalidate after CaptureObservers respect a null
        // dirtyRect input.
        mHandler.post(
                () -> {
                    resourceAdapter.invalidate(null);
                    resourceAdapter.triggerBitmapCapture();
                });
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

        // TODO(crbug.com/1511702): Maybe handle mid-progress pivots for browser controls.
        if (mTransitionFinishedObserver != null) {
            Log.w(
                    TAG,
                    "TransitionFinishedObserver is not cleared when new transition starts. This"
                            + " means previous transition was not finished properly.");
            notifyTransitionFinished(false);
            recordTabStripTransitionFinished(false);
        }

        // TODO(crbug.com/1509013): Request directly instead of using observer interface.
        for (var observer : mTabStripHeightObservers) {
            observer.onTransitionRequested(newHeight);
        }

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
            return;
        }

        if (mTransitionKickoffObserver != null) return;
        mTransitionKickoffObserver =
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
                };
        mBrowserControlsVisibilityManager.addObserver(mTransitionKickoffObserver);
    }

    // TODO(crbug.com/1498252): Find a better place to set these top margins.
    private void updateTabStripHeightImpl() {
        // Remove the mBrowserControlsObserver, to make sure this method is called only once.
        if (mTransitionKickoffObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mTransitionKickoffObserver);
            mTransitionKickoffObserver = null;
        }

        // Change the height when we change the margin, to reflect the actual
        // tab strip height. Check the height to make sure this is only called once.
        int height = mTabStripVisible ? mTabStripHeightFromResource : 0;
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

        for (var observer : mTabStripHeightObservers) {
            observer.onHeightChanged(mTabStripHeight);
        }

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
                            boolean needsAnimate) {
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

    /** Get the min screen width required in DP for the tab strip to become visible. */
    private static int getScreenWidthThresholdDp() {
        if (sMinScreenWidthForTesting != null) return sMinScreenWidthForTesting;
        return ToolbarFeatures.DTC_TRANSITION_THRESHOLD_DP.getValue();
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

        // TODO(crbug.com/1511702): Dispatch the transition finished signal sooner
        //  when interruption is detected.
        return topControlsAtSteadyState || browserControlsInvisible;
    }

    private void notifyTransitionFinished(boolean measureControlContainer) {
        mBrowserControlsVisibilityManager.removeObserver(mTransitionFinishedObserver);
        mTransitionFinishedObserver = null;
        for (var observer : mTabStripHeightObservers) {
            observer.onTransitionFinished();
        }

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
                            controlContainerView()
                                    .setMinimumHeight(mToolbarLayout.getHeight() + mTabStripHeight);
                            ViewUtils.requestLayout(
                                    controlContainerView(),
                                    "TabStripTransitionCoordinator.remeasureControlContainer");
                        }));
    }

    private void recordTabStripTransitionFinished(boolean finished) {
        RecordHistogram.recordBooleanHistogram(
                "Android.DynamicTopChrome.TabStripTransition.Finished", finished);
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
