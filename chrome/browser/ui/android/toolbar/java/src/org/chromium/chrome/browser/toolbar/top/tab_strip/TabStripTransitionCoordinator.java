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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;

/** Class used to manage tab strip visibility and height updates. */
public class TabStripTransitionCoordinator implements ComponentCallbacks, AppHeaderObserver {
    static Integer sHeightTransitionThresholdForTesting;
    static Integer sFadeTransitionThresholdForTesting;

    // Delay to kickoff the transition to avoid frame drops while application is too busy when the
    // configuration changed.
    private static final int TRANSITION_DELAY_MS = 200;

    /** Observes height of tab strip that could change during run time. */
    // TODO(crbug.com/41481630): Rework the observer interface.
    public interface TabStripHeightObserver {
        /**
         * Called when the tab strip requests an update when control container changes its width.
         *
         * @param newHeight The expected height tab strip will be changed into.
         */
        default void onTransitionRequested(int newHeight) {}
    }

    /** Delegate to enforce tab strip updates when strip transition is requested. */
    public interface TabStripTransitionDelegate {
        /**
         * Called when the tab strip height changed. This height will match the space on top of the
         * toolbar reserved for the tab strip.
         *
         * @param newHeight The height same as {@link #getTabStripHeight()}.
         */
        default void onHeightChanged(int newHeight) {}

        /** Notify when the tab strip height transition is completed by the browser controls. */
        default void onHeightTransitionFinished() {}

        /**
         * Called when the tab strip visibility needs to be updated by updating the tab strip scrim
         * in-place.
         *
         * @param newOpacity The scrim opacity required at the end of the transition.
         * @param durationMs The duration of the transition animation, in ms.
         */
        default void onFadeTransitionRequested(float newOpacity, int durationMs) {}

        /**
         * Called to get the {@link StripVisibilityState} that is applied at the end of the most
         * recent strip height and/or fade transition. This might not reflect an accurate value if
         * the strip visibility is updated outside of such transitions.
         *
         * @return The current {@link StripVisibilityState}.
         */
        // TODO (crbug.com/375698491): Potentially remove this API when the bug is addressed.
        default @StripVisibilityState int getStripVisibilityState() {
            return StripVisibilityState.UNKNOWN;
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
    private boolean mForceUpdateHeight;
    private boolean mForceFadeInStrip;

    private OnLayoutChangeListener mOnLayoutChangedListener;
    private @Nullable Runnable mLayoutTransitionTask;

    // TODO (crbug.com/345849359): Create a base handler class to hold common members.
    private final HeightTransitionHandler mHeightTransitionHandler;
    private final FadeTransitionHandler mFadeTransitionHandler;

    private final OneshotSupplier<TabStripTransitionDelegate> mTabStripTransitionDelegateSupplier;

    /**
     * Create the coordinator to manage transitions to show / hide the tab strip.
     *
     * @param browserControlsVisibilityManager {@link BrowserControlsVisibilityManager} to observe
     *     browser controls height and animation state.
     * @param controlContainer The {@link ControlContainer} for the containing activity.
     * @param toolbarLayout {@link ToolbarLayout} for the current toolbar.
     * @param tabStripHeightFromResource The height of the tab strip defined in resource.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} instance.
     * @param tabStripTransitionDelegateSupplier Supplier for the {@link
     *     TabStripTransitionDelegate}.
     */
    public TabStripTransitionCoordinator(
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            ControlContainer controlContainer,
            View toolbarLayout,
            int tabStripHeightFromResource,
            TabObscuringHandler tabObscuringHandler,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            OneshotSupplier<TabStripTransitionDelegate> tabStripTransitionDelegateSupplier) {
        mControlContainer = controlContainer;
        mTabStripHeightFromResource = tabStripHeightFromResource;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mHandler = new Handler(Looper.getMainLooper());
        mTabStripTransitionDelegateSupplier = tabStripTransitionDelegateSupplier;
        mHeightTransitionHandler =
                new HeightTransitionHandler(
                        browserControlsVisibilityManager,
                        controlContainer,
                        toolbarLayout,
                        tabStripHeightFromResource,
                        mCallbackController,
                        mHandler,
                        tabObscuringHandler,
                        tabStripTransitionDelegateSupplier);
        mFadeTransitionHandler =
                new FadeTransitionHandler(tabStripTransitionDelegateSupplier, mCallbackController);

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
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
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
        boolean desktopWindowingModeChanged = wasInDesktopWindow != isInDesktopWindow;

        // Force trigger the strip height transition when the app is switching desktop windowing
        // mode, to update the strip top padding.
        mForceUpdateHeight = desktopWindowingModeChanged;

        // Force fade in an invisible tab strip when the app is exiting desktop windowing mode, and
        // the height transition is blocked.
        mForceFadeInStrip =
                desktopWindowingModeChanged
                        && mHeightTransitionHandler.isHeightTransitionBlocked()
                        && getStripVisibilityState() == StripVisibilityState.INVISIBLE;

        mAppHeaderState = newState;
        if (mAppHeaderState.isInDesktopWindow()) {
            int height = mAppHeaderState.getAppHeaderHeight();
            int topPadding =
                    Math.max(mTabStripReservedTopPadding, height - mTabStripHeightFromResource);
            onTabStripSizeChanged(mAppHeaderState.getUnoccludedRectWidth(), topPadding);
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

    /** Add observer for tab strip height change. */
    public void addObserver(TabStripHeightObserver observer) {
        mHeightTransitionHandler.addObserver(observer);
    }

    // Tab strip height transition implementation methods.

    /** Remove observer for tab strip height change. */
    public void removeObserver(TabStripHeightObserver observer) {
        mHeightTransitionHandler.removeObserver(observer);
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

        onTabStripSizeChanged(newWidth, mTopPadding);
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
        if (width == mTabStripWidth && topPadding == mTopPadding) return;
        mTabStripWidth = width;
        mTopPadding = topPadding;
        AppHeaderUtils.recordDesktopWindowModeStateEnumHistogram(
                mDesktopWindowStateManager,
                "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState");

        if (mLayoutTransitionTask != null) {
            mHandler.removeCallbacks(mLayoutTransitionTask);
        }

        mLayoutTransitionTask =
                mCallbackController.makeCancelable(() -> initiateTransition(width, topPadding));
        mHandler.postDelayed(mLayoutTransitionTask, TRANSITION_DELAY_MS);
    }

    private void initiateTransition(int width, int topPadding) {
        // Notify the handlers of special scenarios to update strip height and visibility.
        mHeightTransitionHandler.setForceUpdateHeight(mForceUpdateHeight);
        mFadeTransitionHandler.setForceFadeInStrip(mForceFadeInStrip);

        boolean isInDesktopWindow = AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager);
        if (!isInDesktopWindow || mForceUpdateHeight) {
            mHeightTransitionHandler.onTabStripSizeChanged(width, topPadding);
        }

        if (isInDesktopWindow || mForceFadeInStrip) {
            mFadeTransitionHandler.onTabStripSizeChanged(width);
        }

        // Reset internal state after use.
        mForceUpdateHeight = false;
        mForceFadeInStrip = false;
    }

    private @StripVisibilityState int getStripVisibilityState() {
        if (mTabStripTransitionDelegateSupplier == null
                || mTabStripTransitionDelegateSupplier.get() == null) {
            return StripVisibilityState.UNKNOWN;
        }
        return mTabStripTransitionDelegateSupplier.get().getStripVisibilityState();
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

    /**
     * Set the tab strip fade transition threshold for testing.
     *
     * @param transitionThresholdForTesting Threshold for the tab strip to become visible.
     */
    public static void setFadeTransitionThresholdForTesting(int transitionThresholdForTesting) {
        sFadeTransitionThresholdForTesting = transitionThresholdForTesting;
        ResettersForTesting.register(() -> sFadeTransitionThresholdForTesting = null);
    }

    HeightTransitionHandler getHeightTransitionHandlerForTesting() {
        return mHeightTransitionHandler;
    }
}
