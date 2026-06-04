// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.View;

import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
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

    private final OneshotSupplier<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier;

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
     * The tab strip was suppressed by {@link suppressTapStrip()}. The method is called when the tab
     * strip needs hiding in favor of other UI such as vertical tab.
     */
    private boolean mTabStripSuppressed;

    /** Tracks the last width seen for the tab strip. */
    private int mTabStripWidth;

    /** Tracks the additional top padding added to the tab strip. */
    private int mTopPadding;

    private boolean mIsDestroyed;

    private boolean mForceUpdateHeight;
    private boolean mUpdateStripVisibility;

    /**
     * Create the manager for transitions to show / hide the tab strip by updating the strip height.
     *
     * @param controlContainer The {@link ControlContainer} for the containing activity.
     * @param tabStripHeightFromResource The height of the tab strip defined in resource.
     * @param callbackController The {@link CallbackController} used by {@link
     *     TabStripTransitionCoordinator}.
     * @param handler The {@link Handler} used by {@link TabStripTransitionCoordinator}.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param tabStripTransitionDelegateSupplier Supplier for the {@link
     *     TabStripTransitionDelegate}.
     * @param tabStripTransitionHandler The {@link TabStripTransitionHandler} instance to facilitate
     *     tab strip visibility transitions.
     */
    HeightTransitionHandler(
            ControlContainer controlContainer,
            int tabStripHeightFromResource,
            CallbackController callbackController,
            Handler handler,
            TabObscuringHandler tabObscuringHandler,
            OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier,
            TabStripTransitionHandler tabStripTransitionHandler) {
        mControlContainer = controlContainer;
        mTabStripHeightFromResource = tabStripHeightFromResource;
        mCallbackController = callbackController;
        mHandler = handler;
        mTabStripTransitionDelegateSupplier = tabStripTransitionDelegateSupplier;
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

    /** Called when the tab strip is suppressed in favor of other UI such as vertical tab. */
    void suppressTabStrip(boolean suppress) {
        mTabStripSuppressed = suppress;
        requestTransition();
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
        if (canForceTransitionDuringStartup()) {
            maybeUpdateTabStripVisibility();
        } else {
            mTabStripTransitionDelegateSupplier.runSyncOrOnAvailable(
                    mCallbackController.makeCancelable(
                            delegate -> maybeUpdateTabStripVisibility()));
        }
    }

    private void maybeUpdateTabStripVisibility() {
        // Do not allow callback to pass through when object is destroyed.
        if (mIsDestroyed) return;

        boolean showTabStrip =
                (mTabStripWidth >= mTabStripTransitionThreshold) && !mTabStripSuppressed;
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

        // When we are force an transition height update during start up, skip waiting for the
        // toolbar capture, since the native scene layer is not ready at this point.
        if (canForceTransitionDuringStartup()) {
            updateTabStrip(showTabStrip);
        }

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

        mTabStripTransitionHandler.onTransitionRequested(
                newHeight,
                mTopPadding,
                mUpdateStripVisibility,
                () -> {
                    // Acknowledge and record the new height when transition start signal.
                    // This difference in timing is necessary, since the mTabStripHeight is used
                    // for other parts of the code to update the browser UI (e.g. setting toolbar's
                    // top margin), and we do not want to preemptively change it before render
                    // responses to the change.
                    mTabStripHeight = newHeight;
                });
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

    /** Whether we can perform a tab strip transition right away, skip waiting on the delegate. */
    private boolean canForceTransitionDuringStartup() {
        if (!BrowserControlsUtils.isForceTopChromeHeightAdjustmentOnStartupEnabled(
                mControlContainer.getView().getContext())) {
            return false;
        }

        if (mTabStripTransitionDelegateSupplier.get() != null) {
            return false;
        }

        // Force update the transition when we are updating app header's height.
        return !mUpdateStripVisibility && mForceUpdateHeight;
    }
}
