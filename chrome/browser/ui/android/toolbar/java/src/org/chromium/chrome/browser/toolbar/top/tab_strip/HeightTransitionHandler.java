// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.View;

import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.Observer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionHandler;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceReadyOnceCallback;
import org.chromium.ui.util.TokenHolder;

/**
 * Owned and used by {@link TabStripTransitionCoordinator} to manage tab strip height transitions
 * that may be triggered by one of the following conditions: 1. Window resizing that warrants
 * showing / hiding the strip by updating its height maintained by the control container. 2. Entry
 * into / exit from desktop windowing mode that warrants the strip height to be updated to include /
 * exclude a reserved strip top padding.
 */
@NullMarked
class HeightTransitionHandler {
    private static final String TAG = "DTCStripTransition";

    // Minimum width (in dp) of the screen for the tab strip to be shown.
    private static final int TRANSITION_THRESHOLD_DP = 412;

    private final TabStripTransitionHandler mTabStripTransitionHandler;
    private final CallbackController mCallbackController;
    private final Handler mHandler;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final ControlContainer mControlContainer;
    private final int mTabStripHeightFromResource;

    // Defer transition token state.
    private final TokenHolder mDeferTransitionTokenHolder;

    /**
     * Internal state used to block the transition until the TRANSITION_DELAY_MS after the last
     * #onLayout pass.
     */
    private int mOnLayoutToken = TokenHolder.INVALID_TOKEN;

    /** Token used to block the transition when URL bar has focus. */
    private int mUrlBarFocusToken = TokenHolder.INVALID_TOKEN;

    private int mTabObscureToken = TokenHolder.INVALID_TOKEN;
    private final TabObscuringHandler mTabObscuringHandler;
    private TabObscuringHandler.@Nullable Observer mTabObscuringHandlerObserver;

    private final TabStripTransitionDelegate mTabStripTransitionDelegate;

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

    private BrowserControlsStateProvider.@Nullable Observer mTransitionKickoffObserver;
    private BrowserControlsStateProvider.@Nullable Observer mTransitionFinishedObserver;

    private boolean mForceUpdateHeight;
    private boolean mUpdateStripVisibility;

    /**
     * Create the manager for transitions to show / hide the tab strip by updating the strip height.
     *
     * @param browserControlsVisibilityManager {@link BrowserControlsVisibilityManager} to observe
     *     browser controls height and animation state.
     * @param controlContainer The {@link ControlContainer} for the containing activity.
     * @param tabStripHeightFromResource The height of the tab strip defined in resource.
     * @param callbackController The {@link CallbackController} used by {@link
     *     TabStripTransitionCoordinator}.
     * @param handler The {@link Handler} used by {@link TabStripTransitionCoordinator}.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param tabStripTransitionDelegate The {@link TabStripTransitionDelegate} to help facilitate
     *     the height transition.
     * @param tabStripTransitionHandler The {@link TabStripTransitionHandler} instance to facilitate
     *     tab strip visibility transitions.
     */
    HeightTransitionHandler(
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            ControlContainer controlContainer,
            int tabStripHeightFromResource,
            CallbackController callbackController,
            Handler handler,
            TabObscuringHandler tabObscuringHandler,
            TabStripTransitionDelegate tabStripTransitionDelegate,
            TabStripTransitionHandler tabStripTransitionHandler) {
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mControlContainer = controlContainer;
        mTabStripHeightFromResource = tabStripHeightFromResource;
        mCallbackController = callbackController;
        mHandler = handler;
        mTabStripTransitionDelegate = tabStripTransitionDelegate;
        mTabStripTransitionHandler = tabStripTransitionHandler;

        mTabStripHeight = tabStripHeightFromResource;
        mTabStripVisible = mTabStripHeight > 0;
        mDeferTransitionTokenHolder =
                new TokenHolder(mCallbackController.makeCancelable(this::onTokenUpdate));

        mTabObscuringHandler = tabObscuringHandler;
        mTabObscuringHandlerObserver =
                (obscureTabContent, obscureToolbar) -> {
                    // Do not block transition if the toolbar is also obscured.
                    if (obscureToolbar) return;

                    if (obscureTabContent) {
                        int token = requestDeferTabStripTransitionToken();
                        if (mTabObscureToken != TokenHolder.INVALID_TOKEN) {
                            releaseTabStripToken(mTabObscureToken);
                        }
                        mTabObscureToken = token;
                    } else {
                        releaseTabStripToken(mTabObscureToken);
                        mTabObscureToken = TokenHolder.INVALID_TOKEN;
                    }
                };
        mTabObscuringHandler.addObserver(mTabObscuringHandlerObserver);
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
        if (mTabObscuringHandlerObserver != null) {
            mTabObscuringHandler.removeObserver(mTabObscuringHandlerObserver);
            mTabObscuringHandlerObserver = null;
        }
    }

    /**
     * Called when URL bar gains / lost focus. When gaining focus, block the tab strip transition.
     */
    // TODO(crbug.com/41492673): Remove this APIs - location bar is also using TabObscuringHandler.
    void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            int token = requestDeferTabStripTransitionToken();
            if (mUrlBarFocusToken != TokenHolder.INVALID_TOKEN) {
                releaseTabStripToken(mUrlBarFocusToken);
            }
            mUrlBarFocusToken = token;
        }
    }

    /** Called when URL bar focus animation finished. Release the token for tab strip transition. */
    void onUrlAnimationFinished(boolean hasFocus) {
        if (!hasFocus) {
            releaseTabStripToken(mUrlBarFocusToken);
            mUrlBarFocusToken = TokenHolder.INVALID_TOKEN;
        }
    }

    /** Return the current tab strip height. */
    int getTabStripHeight() {
        return mTabStripHeight;
    }

    /** Request the token to defer the tab strip transition to a later time. */
    int requestDeferTabStripTransitionToken() {
        return mDeferTransitionTokenHolder.acquireToken();
    }

    /**
     * Release the token acquired from {@link #requestDeferTabStripTransitionToken()} so the tab
     * strip can transition based on its current size.
     */
    void releaseTabStripToken(int token) {
        mDeferTransitionTokenHolder.releaseToken(token);
    }

    void updateTabStripTransitionThreshold(DisplayMetrics displayMetrics) {
        mTabStripTransitionThreshold =
                ViewUtils.dpToPx(displayMetrics, getScreenWidthThresholdDp());

        if (TabStripTransitionCoordinator.sHeightTransitionThresholdForTesting != null) {
            requestTransition();
        }
    }

    private void requestTransition() {
        maybeUpdateTabStripVisibility();
    }

    private void maybeUpdateTabStripVisibility() {
        // Do not allow callback to pass through when object is destroyed.
        if (mIsDestroyed) return;

        boolean showTabStrip = mTabStripWidth >= mTabStripTransitionThreshold;
        if (showTabStrip == mTabStripVisible && !mForceUpdateHeight) {
            // Do not transition if visibility does not change, unless we want to continue the
            // transition to update the tab strip top padding.
            return;
        }

        // Update the min size for the control container. This is needed one-layout-before browser
        // controls start changing its height, as it assumed a fixed size control container during
        // transition. See b/324178484.
        int maxHeight =
                calculateTabStripHeight()
                        + mControlContainer.getToolbarHeight()
                        + mControlContainer.getToolbarHairlineHeight();
        controlContainerView().setMinimumHeight(maxHeight);

        // When transition kicked off by the BrowserControlsManager, the toolbar capture can be
        // stale e.g. still with the previous window width. Force invalidate the toolbar capture to
        // make sure the it's up-to-date with the latest Android view.
        var resourceAdapter = mControlContainer.getToolbarResourceAdapter();
        DynamicResourceReadyOnceCallback.onNext(
                resourceAdapter, (resource) -> updateTabStrip(showTabStrip));

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

    void onTabStripSizeChanged(
            int width, int topPadding, boolean isInDesktopWindow, boolean forceUpdateHeight) {
        if (width == mTabStripWidth && topPadding == mTopPadding) return;
        mTabStripWidth = width;
        mTopPadding = topPadding;

        // Height transition should update the strip visibility only when not in a desktop window.
        // Visibility is dictated by a fade transition in a desktop window.
        mUpdateStripVisibility = !isInDesktopWindow;
        mForceUpdateHeight = forceUpdateHeight;

        if (isInDesktopWindow) {
            // In a desktop window, do not block the height transition when transition token is in
            // use and instead trigger the transition immediately so that the strip top padding is
            // updated as needed.
            requestTransition();
        } else {
            int oldToken = mOnLayoutToken;
            mOnLayoutToken = mDeferTransitionTokenHolder.acquireToken();
            mDeferTransitionTokenHolder.releaseToken(oldToken);
            mDeferTransitionTokenHolder.releaseToken(mOnLayoutToken);
        }
    }

    private void onTokenUpdate() {
        // Block new request for transitions as long as there's any token left.
        if (mDeferTransitionTokenHolder.hasTokens()) return;
        requestTransition();
    }

    private View controlContainerView() {
        return mControlContainer.getView();
    }

    /** This may differ from |mTabStripHeight| when the transition is about to start. */
    private int calculateTabStripHeight() {
        return mTabStripHeightFromResource + mTopPadding;
    }

    /**
     * Set the new height for the tab strip. This on high level consists of 3 steps:
     *
     * <ul>
     *   <li>1. Use BrowserControlsSizer to change the browser control height. This will kick off
     *       the scene layer transition.
     *   <li>2. Add / remove margins from the toolbar and toolbar hairline based on tab strip's new
     *       height.
     *   <li>3. Notify the tab strip scene layer for the new height. This will in turn notify
     *       StripLayoutHelperManager that will: 1) Update the strip visibility by marking a yOffset
     *       for the strip to render off-screen. We cannot simply mark the tab strip scene layer
     *       hidden, since it's still required to be visible during the transition. and/or 2) Update
     *       the strip's top padding, relevant when the app header is used.
     * </ul>
     *
     * @param showTabStrip Whether the tab strip should be shown, based on the window width. Note
     *     that this will be used and applied only when the app is not in a desktop window.
     */
    private void updateTabStrip(boolean showTabStrip) {
        if (mIsDestroyed) return;

        mTabStripVisible = showTabStrip;
        int newHeight =
                (mUpdateStripVisibility && showTabStrip)
                                || (!mUpdateStripVisibility && mForceUpdateHeight)
                        ? calculateTabStripHeight()
                        : 0;

        // TODO(crbug.com/41484284): Maybe handle mid-progress pivots for browser controls.
        if (mTransitionFinishedObserver != null) {
            Log.w(
                    TAG,
                    "TransitionFinishedObserver is not cleared when new transition starts. This"
                            + " means previous transition was not finished properly.");
            notifyTransitionFinished(false);
            recordTabStripTransitionFinished(false);
        }

        mTabStripTransitionHandler.onTransitionRequested(
                newHeight,
                mUpdateStripVisibility,
                () -> {
                    // Acknowledge and record the new height when transition start signal.
                    // This difference in timing is necessary, since the mTabStripHeight is used
                    // for other parts of the code to update the browser UI (e.g. setting toolbar's
                    // top margin), and we do not want to preemptively change it before render
                    // responses to the change.
                    mTabStripHeight = newHeight;
                });

        // When isTopControlsRefactorOffsetEnabled is true, the mTabStripTransitionHandler will
        // drive and handle the transition, so the business logic for mTransitionFinishedObserver
        // and mTransitionKickoffObserver should be skipped.
        if (BrowserControlsUtils.isTopControlsRefactorOffsetEnabled()) return;

        // If the browser control is performing an browser initiated animation, we should update the
        // view margins right away. This will make sure the toolbar stays in the same place with
        // changes in control container's Y translation.
        boolean javaAnimationInProgress = mBrowserControlsVisibilityManager.offsetOverridden();

        // For cases where transition is finished in sequence during #onTransitionRequested (e.g.
        // browser control's visibility is under constraint), we'll call updateTabStripHeightImpl
        // to update the margin for the views.
        Integer visibility = mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate().get();
        assumeNonNull(visibility);
        boolean browserControlsHasConstraint = visibility != BrowserControlsState.BOTH;

        if (javaAnimationInProgress || browserControlsHasConstraint) {
            updateTabStripHeightImpl(newHeight);
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
                            boolean topControlsMinHeightChanged,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean bottomControlsMinHeightChanged,
                            boolean requestNewFrame,
                            boolean isVisibilityForced) {
                        updateTabStripHeightImpl(newHeight);
                    }

                    @Override
                    public void onAndroidControlsVisibilityChanged(int visibility) {
                        // Update the margin when browser control turns into invisible. This can
                        // happen before onControlsOffsetChanged.
                        updateTabStripHeightImpl(newHeight);
                    }
                };
        mBrowserControlsVisibilityManager.addObserver(mTransitionKickoffObserver);
    }

    private void updateTabStripHeightImpl(int height) {
        // Remove the mBrowserControlsObserver, to make sure this method is called only once.
        if (mTransitionKickoffObserver != null) {
            mBrowserControlsVisibilityManager.removeObserver(mTransitionKickoffObserver);
            mTransitionKickoffObserver = null;
        }

        // Change the height when we change the margin, to reflect the actual
        // tab strip height. Check the height to make sure this is only called once.
        if (mTabStripHeight == height) return;
        mTabStripHeight = height;

        mControlContainer.onHeightChanged(height, mUpdateStripVisibility);

        // The height transition should apply the strip scrim overlay only when its goal is to
        // update the strip visibility. In a desktop window, the height transition runs solely
        // to update the strip top padding and it is expected of the fade transition to
        // control the strip visibility by updating the scrim in this case when applicable.
        mTabStripTransitionDelegate.onHeightChanged(mTabStripHeight, mUpdateStripVisibility);

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
                            boolean topControlsMinHeightChanged,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean bottomControlsMinHeightChanged,
                            boolean requestNewFrame,
                            boolean isVisibilityForced) {
                        if (isTopControlAtSteadyState()) {
                            notifyTransitionFinished(true);
                            recordTabStripTransitionFinished(true);
                        }
                    }
                };
        mBrowserControlsVisibilityManager.addObserver(mTransitionFinishedObserver);
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

    private void notifyTransitionFinished(boolean finished) {
        assumeNonNull(mTransitionFinishedObserver);
        mBrowserControlsVisibilityManager.removeObserver(mTransitionFinishedObserver);
        mTransitionFinishedObserver = null;

        mTabStripTransitionDelegate.onHeightTransitionFinished(finished);
        mControlContainer.onHeightTransitionFinished(finished);
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

        if (CommandLine.isInitialized()) {
            String commandLineThreshold =
                    CommandLine.getInstance()
                            .getSwitchValue(
                                    ChromeSwitches.TAB_STRIP_HEIGHT_TRANSITION_THRESHOLD, "0");
            int threshold = Integer.parseInt(commandLineThreshold);
            if (threshold > 0) return threshold;
        }

        return TRANSITION_THRESHOLD_DP;
    }

    boolean isHeightTransitionBlocked() {
        return mDeferTransitionTokenHolder.hasTokens();
    }
}
