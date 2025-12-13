// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.View.OnLayoutChangeListener;

import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;

/**
 * Class used to manage tab strip visibility and height updates.
 *
 * <p>As of Nov 2025, this class is refactored to only request the tab strip height transition when
 * applicable. The movement of the tab strip as a result of this transition is controlled by the
 * TabStripTopControlLayer.
 */
@NullMarked
public class TabStripTransitionCoordinator implements ComponentCallbacks, AppHeaderObserver {
    static @Nullable Integer sHeightTransitionThresholdForTesting;

    // Delay to kickoff the transition to avoid frame drops while application is too busy when the
    // configuration changed.
    private static final int TRANSITION_DELAY_MS = 200;

    /**
     * Interface that exposes methods to handle tab strip height transitions that can impact strip
     * visibility.
     */
    public interface TabStripTransitionHandler {
        /**
         * Called when the tab strip requests an update when control container changes its width.
         *
         * @param newHeight The expected height tab strip will be changed into.
         * @param applyScrimOverlay Whether the strip scrim should be updated during the transition.
         * @param transitionStartedCallback The callback to trigger when transition has started.
         *     This is not guaranteed to be called.
         */
        default void onTransitionRequested(
                int newHeight, boolean applyScrimOverlay, Runnable transitionStartedCallback) {}
    }

    /** Delegate to enforce tab strip updates when strip transition is requested. */
    public interface TabStripTransitionDelegate {
        /**
         * Called when the tab strip height changed. This height will match the space on top of the
         * toolbar reserved for the tab strip.
         *
         * @param newHeight The height same as {@link #getTabStripHeight()}.
         * @param applyScrimOverlay Whether the strip scrim should be updated during the transition.
         *     {@code true} when the transition expects to update the strip visibility, {@code
         *     false} otherwise.
         */
        default void onHeightChanged(int newHeight, boolean applyScrimOverlay) {}

        /**
         * Notify when the tab strip height transition is completed by the browser controls.
         *
         * @param success Whether the transition succeeded (true) or was canceled (false).
         */
        default void onHeightTransitionFinished(boolean success) {}

        /**
         * Called when the tab strip visibility needs to be updated by updating the tab strip scrim
         * in-place.
         *
         * @param newOpacity The scrim opacity required at the end of the transition.
         * @param durationMs The duration of the transition animation, in ms.
         */
        default void onFadeTransitionRequested(float newOpacity, int durationMs) {}

        /** Returns whether the tab strip is hidden by the fade transition. */
        default boolean isHiddenByFadeTransition() {
            return false;
        }

        /**
         * Returns the min strip width (in dp) required for it to become visible by a fade
         * transition.
         */
        default int getFadeTransitionThresholdDp() {
            return 0;
        }
    }

    private final CallbackController mCallbackController = new CallbackController();
    private final Handler mHandler;
    private final ControlContainer mControlContainer;
    private final int mTabStripHeightFromResource;
    private final int mTabStripReservedTopPadding;

    /** Tracks the last width seen for the tab strip. */
    private int mTabStripWidth;

    /** Tracks the additional top padding added to the tab strip. */
    private int mTopPadding;

    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private @Nullable AppHeaderState mAppHeaderState;
    private boolean mDesktopWindowingModeChanged;
    private boolean mForceUpdateHeight;
    private boolean mForceFadeInStrip;

    private @Nullable OnLayoutChangeListener mOnLayoutChangedListener;
    private @Nullable Runnable mLayoutTransitionTask;

    // TODO (crbug.com/345849359): Create a base handler class to hold common members.
    private final HeightTransitionHandler mHeightTransitionHandler;
    private final FadeTransitionHandler mFadeTransitionHandler;

    private final TabStripTransitionDelegate mTabStripTransitionDelegate;

    /**
     * Create the coordinator to manage transitions to show / hide the tab strip.
     *
     * @param browserControlsVisibilityManager {@link BrowserControlsVisibilityManager} to observe
     *     browser controls height and animation state.
     * @param controlContainer The {@link ControlContainer} for the containing activity.
     * @param tabStripHeightFromResource The height of the tab strip defined in resource.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} instance.
     * @param tabStripTransitionDelegateSupplier Supplier for the {@link
     *     TabStripTransitionDelegate}.
     * @param tabStripTransitionHandler The {@link TabStripTransitionHandler} instance to facilitate
     *     tab strip visibility transitions.
     */
    public TabStripTransitionCoordinator(
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            ControlContainer controlContainer,
            int tabStripHeightFromResource,
            TabObscuringHandler tabObscuringHandler,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            TabStripTransitionDelegate tabStripTransitionDelegate,
            TabStripTransitionHandler tabStripTransitionHandler) {
        mControlContainer = controlContainer;
        mTabStripHeightFromResource = tabStripHeightFromResource;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mHandler = new Handler(Looper.getMainLooper());
        mTabStripTransitionDelegate = tabStripTransitionDelegate;
        mHeightTransitionHandler =
                new HeightTransitionHandler(
                        browserControlsVisibilityManager,
                        controlContainer,
                        tabStripHeightFromResource,
                        mCallbackController,
                        mHandler,
                        tabObscuringHandler,
                        tabStripTransitionDelegate,
                        tabStripTransitionHandler);
        mFadeTransitionHandler =
                new FadeTransitionHandler(tabStripTransitionDelegate, mCallbackController);

        mTabStripReservedTopPadding =
                controlContainerView()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_reserved_top_padding);

        mOnLayoutChangedListener =
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    int windowWidth = Math.abs(right - left);
                    onLayoutWidthChanged(windowWidth);
                };
        controlContainerView().addOnLayoutChangeListener(mOnLayoutChangedListener);

        updateTabStripTransitionThreshold();

        AppHeaderState appHeaderState = null;
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(this);
            appHeaderState = mDesktopWindowStateManager.getAppHeaderState();
        }

        // Initialize the tab strip size based on whether we have app header.
        if (appHeaderState != null) {
            onAppHeaderStateChanged(appHeaderState);
        } else {
            onLayoutWidthChanged(controlContainerView().getWidth());
        }
    }

    // ComponentCallbacks implementation.

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        updateTabStripTransitionThreshold();
    }

    @Override
    public void onLowMemory() {}

    // DesktopWindowStateManager.AppHeaderObserver implementation.

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        assert mDesktopWindowStateManager != null;
        assert newState != null;

        boolean wasInDesktopWindow = mAppHeaderState != null && mAppHeaderState.isInDesktopWindow();
        boolean isInDesktopWindow = newState.isInDesktopWindow();
        mDesktopWindowingModeChanged = wasInDesktopWindow != isInDesktopWindow;
        boolean headerHeightChanged =
                mAppHeaderState != null
                        && mAppHeaderState.getAppHeaderHeight() != newState.getAppHeaderHeight();

        // Force trigger the strip height transition when:
        // 1. The app is switching desktop windowing mode, to update the strip top padding.
        // 2. The app header height changes.
        mForceUpdateHeight = mDesktopWindowingModeChanged || headerHeightChanged;

        // Force fade in an invisible tab strip when the app is exiting desktop windowing mode, and
        // the height transition is blocked.
        mForceFadeInStrip =
                mDesktopWindowingModeChanged
                        && mHeightTransitionHandler.isHeightTransitionBlocked()
                        && isTabStripHiddenByFadeTransition();

        mAppHeaderState = newState;
        if (mAppHeaderState.isInDesktopWindow()) {
            onTabStripSizeChanged(mAppHeaderState.getUnoccludedRectWidth(), calculateTopPadding());
        } else {
            onTabStripSizeChanged(controlContainerView().getWidth(), 0);
        }
    }

    /** Remove observers and release reference to dependencies. */
    public void destroy() {
        if (mOnLayoutChangedListener != null) {
            controlContainerView().removeOnLayoutChangeListener(mOnLayoutChangedListener);
            mOnLayoutChangedListener = null;
        }
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(this);
        }
        mCallbackController.destroy();
        mHeightTransitionHandler.destroy();
    }

    /** Return the current tab strip height. */
    public int getTabStripHeight() {
        return mHeightTransitionHandler.getTabStripHeight();
    }

    /** Request the token to defer the tab strip height transition to a later time. */
    public int requestDeferTabStripTransitionToken() {
        return mHeightTransitionHandler.requestDeferTabStripTransitionToken();
    }

    /**
     * Release the token acquired from {@link #requestDeferTabStripTransitionToken()} so the tab
     * strip can height-transition based on its current size.
     */
    public void releaseTabStripToken(int token) {
        mHeightTransitionHandler.releaseTabStripToken(token);
    }

    /**
     * Called when URL bar gains / lost focus. When gaining focus, block the tab strip height
     * transition.
     */
    // TODO(crbug.com/41492673): Remove this APIs - location bar is also using TabObscuringHandler.
    public void onUrlFocusChange(boolean hasFocus) {
        mHeightTransitionHandler.onUrlFocusChange(hasFocus);
    }

    /**
     * Called when URL bar focus animation finished. Release the token for tab strip height
     * transition.
     */
    public void onUrlAnimationFinished(boolean hasFocus) {
        mHeightTransitionHandler.onUrlAnimationFinished(hasFocus);
    }

    private void updateTabStripTransitionThreshold() {
        DisplayMetrics displayMetrics = controlContainerView().getResources().getDisplayMetrics();
        mHeightTransitionHandler.updateTabStripTransitionThreshold(displayMetrics);
        mFadeTransitionHandler.updateTabStripTransitionThreshold(displayMetrics);
    }

    private View controlContainerView() {
        return mControlContainer.getView();
    }

    private void onLayoutWidthChanged(int newWidth) {
        // If mAppHeaderState exists, check the widestUnoccludedRect too. This is needed as
        // updates in mAppHeaderState can happen prior / during a layout pass, while the
        // transition needs to wait until UI is in a stable state.
        if (mAppHeaderState != null && mAppHeaderState.getUnoccludedRectWidth() > 0) {
            newWidth = Math.min(newWidth, mAppHeaderState.getUnoccludedRectWidth());
        }

        onTabStripSizeChanged(newWidth, calculateTopPadding());
    }

    /**
     * Request a strip height and/or fade transition as applicable, when the tab strip size changes.
     * Always wait for a short delay after the last #onLayout pass for the control container to make
     * sure the UI is in a stable state.
     *
     * @param width The current width of tab strip.
     * @param topPadding The top padding to be added to the tab strip.
     */
    private void onTabStripSizeChanged(int width, int topPadding) {
        // Avoid transitioning when strip width / control container height is invalid. This can
        // happen when the control container is created hidden after theme changes.
        if (width <= 0 || controlContainerView().getHeight() == 0) return;

        if (width == mTabStripWidth && topPadding == mTopPadding) return;
        mTabStripWidth = width;
        mTopPadding = topPadding;
        AppHeaderUtils.recordDesktopWindowModeStateEnumHistogram(
                mDesktopWindowStateManager,
                "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState");

        if (mLayoutTransitionTask != null) {
            mHandler.removeCallbacks(mLayoutTransitionTask);
        }

        boolean isInDesktopWindow = AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager);
        if (mDesktopWindowingModeChanged) {
            // When desktop windowing mode is changing, avoid posting the transition and trigger
            // it immediately because it has been observed in the past that the post task may be
            // dropped during the switch, thereby not using and enforcing the desired state in a
            // series of invocation of this method in this scenario.
            initiateTransition(
                    width,
                    topPadding,
                    isInDesktopWindow,
                    mForceUpdateHeight,
                    mForceFadeInStrip,
                    mDesktopWindowingModeChanged,
                    mHeightTransitionHandler,
                    mFadeTransitionHandler);
        } else {
            mLayoutTransitionTask =
                    mCallbackController.makeCancelable(
                            () ->
                                    initiateTransition(
                                            width,
                                            topPadding,
                                            isInDesktopWindow,
                                            mForceUpdateHeight,
                                            mForceFadeInStrip,
                                            mDesktopWindowingModeChanged,
                                            mHeightTransitionHandler,
                                            mFadeTransitionHandler));
            mHandler.postDelayed(mLayoutTransitionTask, TRANSITION_DELAY_MS);
        }
    }

    private static void initiateTransition(
            int width,
            int topPadding,
            boolean isInDesktopWindow,
            boolean forceUpdateHeight,
            boolean forceFadeInStrip,
            boolean desktopWindowingModeChanged,
            HeightTransitionHandler heightTransitionHandler,
            FadeTransitionHandler fadeTransitionHandler) {
        boolean runHeightTransition = !isInDesktopWindow || forceUpdateHeight;
        boolean runFadeTransition = isInDesktopWindow || forceFadeInStrip;

        if (runHeightTransition) {
            heightTransitionHandler.onTabStripSizeChanged(
                    width, topPadding, isInDesktopWindow, forceUpdateHeight);
        }

        if (runFadeTransition) {
            fadeTransitionHandler.onTabStripSizeChanged(
                    width, forceFadeInStrip, desktopWindowingModeChanged);
        }
    }

    private boolean isTabStripHiddenByFadeTransition() {
        return mTabStripTransitionDelegate.isHiddenByFadeTransition();
    }

    private int calculateTopPadding() {
        if (mAppHeaderState == null) return 0;
        int height = mAppHeaderState.getAppHeaderHeight();
        return height == 0
                ? 0
                : Math.max(mTabStripReservedTopPadding, height - mTabStripHeightFromResource);
    }

    // Testing methods.

    /**
     * Set the tab strip height transition threshold for testing.
     *
     * @param transitionThresholdForTesting Threshold for the tab strip to become visible.
     */
    public static void setHeightTransitionThresholdForTesting(int transitionThresholdForTesting) {
        sHeightTransitionThresholdForTesting = transitionThresholdForTesting;
        ResettersForTesting.register(() -> sHeightTransitionThresholdForTesting = null);
    }

    HeightTransitionHandler getHeightTransitionHandlerForTesting() {
        return mHeightTransitionHandler;
    }
}
