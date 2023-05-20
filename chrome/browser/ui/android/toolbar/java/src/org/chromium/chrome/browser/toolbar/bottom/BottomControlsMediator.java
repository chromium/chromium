// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom controls component,
 * and updating the model accordingly.
 */
class BottomControlsMediator implements BrowserControlsStateProvider.Observer,
                                        KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                                        LayoutStateObserver, TabObscuringHandler.Observer {
    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    /** The fullscreen manager to observe fullscreen events. */
    private final FullscreenManager mFullscreenManager;

    /** The browser controls sizer/manager to observe browser controls events. */
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final TabObscuringHandler mTabObscuringHandler;

    private final CallbackController mCallbackController;

    /**
     * The height of the bottom bar in pixels, not including the top shadow.
     */
    private int mBottomControlsHeight;

    /** A {@link WindowAndroid} for watching keyboard visibility events. */
    private final WindowAndroid mWindowAndroid;

    /** The bottom controls visibility. */
    private boolean mIsBottomControlsVisible;

    /** Whether any overlay panel is showing. */
    private boolean mIsOverlayPanelShowing;

    /** Whether the swipe layout is currently active. */
    private boolean mIsInSwipeLayout;

    /** Whether the soft keyboard is visible. */
    private boolean mIsKeyboardVisible;

    private LayoutStateProvider mLayoutStateProvider;

    /**
     * Build a new mediator that handles events from outside the bottom controls component.
     * @param windowAndroid A {@link WindowAndroid} for watching keyboard visibility events.
     * @param model The {@link BottomControlsProperties} that holds all the view state for the
     *         bottom controls component.
     * @param controlsSizer The {@link BrowserControlsSizer} to manipulate browser controls.
     * @param fullscreenManager A {@link FullscreenManager} for events related to the browser
     *                          controls.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param bottomControlsHeight The height of the bottom bar in pixels.
     * @param overlayPanelVisibilitySupplier Notifies overlay panel visibility event.
     */
    BottomControlsMediator(WindowAndroid windowAndroid, PropertyModel model,
            BrowserControlsSizer controlsSizer, FullscreenManager fullscreenManager,
            TabObscuringHandler tabObscuringHandler, int bottomControlsHeight,
            ObservableSupplier<Boolean> overlayPanelVisibilitySupplier) {
        mModel = model;

        mFullscreenManager = fullscreenManager;
        mBrowserControlsSizer = controlsSizer;
        mBrowserControlsSizer.addObserver(this);
        mTabObscuringHandler = tabObscuringHandler;
        tabObscuringHandler.addObserver(this);

        mBottomControlsHeight = bottomControlsHeight;
        mCallbackController = new CallbackController();
        overlayPanelVisibilitySupplier.addObserver(mCallbackController.makeCancelable((showing) -> {
            mIsOverlayPanelShowing = showing;
            updateAndroidViewVisibility();
        }));

        // Watch for keyboard events so we can hide the bottom toolbar when the keyboard is showing.
        mWindowAndroid = windowAndroid;
        mWindowAndroid.getKeyboardDelegate().addKeyboardVisibilityListener(this);
    }

    void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        layoutStateProvider.addObserver(this);
    }

    void setBottomControlsVisible(boolean visible) {
        mIsBottomControlsVisible = visible;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    /**
     * Clean up anything that needs to be when the bottom controls component is destroyed.
     */
    void destroy() {
        mCallbackController.destroy();
        mBrowserControlsSizer.removeObserver(this);
        mWindowAndroid.getKeyboardDelegate().removeKeyboardVisibilityListener(this);
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(this);
            mLayoutStateProvider = null;
        }
        mTabObscuringHandler.removeObserver(this);
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        mModel.set(BottomControlsProperties.Y_OFFSET, bottomOffset);
        updateAndroidViewVisibility();
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        mIsKeyboardVisible = isShowing;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    // LayoutStateObserver

    @Override
    public void onStartedShowing(@LayoutType int layoutType) {
        mIsInSwipeLayout = layoutType == LayoutType.TOOLBAR_SWIPE;
        updateAndroidViewVisibility();
    }

    /**
     * @return Whether the browser is currently in fullscreen mode.
     */
    private boolean isInFullscreenMode() {
        return mFullscreenManager != null && mFullscreenManager.getPersistentFullscreenMode();
    }

    /**
     * The composited view is the composited version of the Android View. It is used to be able to
     * scroll the bottom controls off-screen synchronously. Since the bottom controls live below
     * the webcontents we re-size the webcontents through
     * {@link BrowserControlsSizer#setBottomControlsHeight(int,int)} whenever the composited view
     * visibility changes.
     */
    private void updateCompositedViewVisibility() {
        final boolean isCompositedViewVisible = isCompositedViewVisible();
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, isCompositedViewVisible);
        mBrowserControlsSizer.setBottomControlsHeight(
                isCompositedViewVisible ? mBottomControlsHeight : 0,
                mBrowserControlsSizer.getBottomControlsMinHeight());
    }

    boolean isCompositedViewVisible() {
        return mIsBottomControlsVisible && !mIsKeyboardVisible && !isInFullscreenMode();
    }

    /**
     * The Android View is the interactive view. The composited view should always be behind the
     * Android view which means we hide the Android view whenever the composited view is hidden.
     * We also hide the Android view as we are scrolling the bottom controls off screen this is
     * done by checking if {@link BrowserControlsSizer#getBottomControlOffset()} is
     * non-zero.
     */
    private void updateAndroidViewVisibility() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE,
                isCompositedViewVisible() && !mIsOverlayPanelShowing && !mIsInSwipeLayout
                        && mBrowserControlsSizer.getBottomControlOffset() == 0);
    }

    @Override
    public void updateObscured(boolean obscureTabContent, boolean obscureToolbar) {
        mModel.set(BottomControlsProperties.IS_OBSCURED, obscureToolbar);
    }
}
