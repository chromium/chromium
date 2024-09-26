// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom controls component,
 * and updating the model accordingly.
 */
class BottomControlsMediator
        implements BrowserControlsStateProvider.Observer,
                KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                LayoutStateObserver,
                TabObscuringHandler.Observer,
                BottomControlsLayer {
    private static final String TAG = "BotControlsMediator";

    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    /** The fullscreen manager to observe fullscreen events. */
    private final FullscreenManager mFullscreenManager;

    /** The browser controls sizer/manager to observe browser controls events. */
    private final BottomControlsStacker mBottomControlsStacker;

    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final TabObscuringHandler mTabObscuringHandler;

    private final CallbackController mCallbackController;

    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;

    private final Supplier<Boolean> mReadAloudRestoringSupplier;

    /** The height of the bottom bar in pixels, not including the top shadow. */
    private int mBottomControlsHeight;

    /** A {@link WindowAndroid} for watching keyboard visibility events. */
    private final WindowAndroid mWindowAndroid;

    /** The bottom controls visibility. */
    private boolean mIsBottomControlsVisible;

    /** The background color for the bottom controls. */
    private @ColorInt int mBottomControlsColor;

    /** Whether any overlay panel is showing. */
    private boolean mIsOverlayPanelShowing;

    /** Whether the swipe layout is currently active. */
    private boolean mIsInSwipeLayout;

    /** Whether the soft keyboard is visible. */
    private boolean mIsKeyboardVisible;

    private LayoutStateProvider mLayoutStateProvider;

    @Nullable private ChangeObserver mEdgeToEdgeChangeObserver;
    private int mEdgeToEdgePaddingPx;

    /**
     * Build a new mediator that handles events from outside the bottom controls component.
     *
     * @param windowAndroid A {@link WindowAndroid} for watching keyboard visibility events.
     * @param model The {@link BottomControlsProperties} that holds all the view state for the
     *     bottom controls component.
     * @param controlsStacker The {@link BottomControlsStacker} to manipulate browser controls.
     * @param fullscreenManager A {@link FullscreenManager} for events related to the browser
     *     controls.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param bottomControlsHeight The height of the bottom bar in pixels.
     * @param overlayPanelVisibilitySupplier Notifies overlay panel visibility event.
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController} to adjust the
     *     height of the bottom controls when drawing all the way to the edge of the screen.
     * @param readAloudRestoringSupplier Supplier that returns true if Read Aloud is currently
     *     restoring its player, e.g. after theme change.
     */
    BottomControlsMediator(
            WindowAndroid windowAndroid,
            PropertyModel model,
            BottomControlsStacker controlsStacker,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            FullscreenManager fullscreenManager,
            TabObscuringHandler tabObscuringHandler,
            int bottomControlsHeight,
            ObservableSupplier<Boolean> overlayPanelVisibilitySupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            Supplier<Boolean> readAloudRestoringSupplier) {
        mModel = model;

        mFullscreenManager = fullscreenManager;
        mBottomControlsStacker = controlsStacker;
        getBrowserControls().addObserver(this);
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
        mTabObscuringHandler = tabObscuringHandler;
        tabObscuringHandler.addObserver(this);

        mBottomControlsHeight = bottomControlsHeight;
        mCallbackController = new CallbackController();
        overlayPanelVisibilitySupplier.addObserver(
                mCallbackController.makeCancelable(
                        (showing) -> {
                            mIsOverlayPanelShowing = showing;
                            updateAndroidViewVisibility();
                        }));

        // Watch for keyboard events so we can hide the bottom toolbar when the keyboard is showing.
        mWindowAndroid = windowAndroid;
        mWindowAndroid.getKeyboardDelegate().addKeyboardVisibilityListener(this);

        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        if (mEdgeToEdgeControllerSupplier.get() != null) {
            mEdgeToEdgeChangeObserver = this::onEdgeToEdgeChanged;
            mEdgeToEdgeControllerSupplier.get().registerObserver(mEdgeToEdgeChangeObserver);
        }
        mReadAloudRestoringSupplier = readAloudRestoringSupplier;
        mBottomControlsStacker.addLayer(this);
    }

    void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        layoutStateProvider.addObserver(this);
    }

    void setBottomControlsVisible(boolean visible) {
        boolean visibilityChanged = mIsBottomControlsVisible != visible;
        mIsBottomControlsVisible = visible;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();

        // When tab group UI changed from hidden -> visible, request browser controls to show
        // transiently. This is a workaround to when tab is opened in background with a new tab
        // group, the offsets in TabBrowserControlsOffsetHelper is stale. See crbug.com/357398783
        if (visible && visibilityChanged) {
            mBrowserControlsVisibilityDelegate.showControlsTransient();
        }
    }

    void setBottomControlsColor(@ColorInt int color) {
        mBottomControlsColor = color;
    }

    /** Clean up anything that needs to be when the bottom controls component is destroyed. */
    void destroy() {
        mCallbackController.destroy();
        getBrowserControls().removeObserver(this);
        mBottomControlsStacker.removeLayer(this);
        mWindowAndroid.getKeyboardDelegate().removeKeyboardVisibilityListener(this);
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(this);
            mLayoutStateProvider = null;
        }
        if (mEdgeToEdgeControllerSupplier.get() != null && mEdgeToEdgeChangeObserver != null) {
            mEdgeToEdgeControllerSupplier.get().unregisterObserver(mEdgeToEdgeChangeObserver);
            mEdgeToEdgeChangeObserver = null;
        }
        mTabObscuringHandler.removeObserver(this);
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        // Method call routed to onBrowserControlsOffsetUpdate.
        if (BottomControlsStacker.isDispatchingYOffset()) return;

        setYOffset(bottomOffset - getBrowserControls().getBottomControlsMinHeight());
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        // TODO(331829509): Set position in a way that doesn't rely on browser controls size system.
        // Normally our Android view is translated at the end of bottom controls min height
        // animations to place its bottom edge at the min height. This doesn't work during theme
        // change because onControlsOffsetChanged() is never called in that case. Instead we have
        // this special case to make sure the bottom controls aren't covered by the Read Aloud
        // player when it is shown again following browser UI being recreated.
        if (mReadAloudRestoringSupplier.get()) {
            mModel.set(
                    BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y,
                    mModel.get(BottomControlsProperties.Y_OFFSET));
        }
        // A min height greater than 0 suggests the presence of some other UI component underneath
        // the bottom controls.
        if (bottomControlsMinHeight == 0) {
            mBottomControlsStacker.notifyBackgroundColor(mBottomControlsColor);
        }
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

    private void onEdgeToEdgeChanged(
            int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
        mEdgeToEdgePaddingPx = isDrawingToEdge ? bottomInset : 0;

        updateBrowserControlsHeight();

        int androidViewHeight = getAndroidViewHeight();
        if (androidViewHeight != mModel.get(BottomControlsProperties.ANDROID_VIEW_HEIGHT)) {
            mModel.set(BottomControlsProperties.ANDROID_VIEW_HEIGHT, androidViewHeight);
        }
    }

    /**
     * @return Whether the browser is currently in fullscreen mode.
     */
    private boolean isInFullscreenMode() {
        return mFullscreenManager != null && mFullscreenManager.getPersistentFullscreenMode();
    }

    private void setYOffset(int yOffset) {
        mModel.set(BottomControlsProperties.Y_OFFSET, yOffset);

        // This call also updates the view's position if the animation has just finished.
        updateAndroidViewVisibility();
    }

    /**
     * The composited view is the composited version of the Android View. It is used to be able to
     * scroll the bottom controls off-screen synchronously. Since the bottom controls live below the
     * webcontents we re-size the webcontents through {@link
     * BottomControlsStacker#setBottomControlsHeight(int, int, boolean)} whenever the composited
     * view visibility changes.
     */
    private void updateCompositedViewVisibility() {
        final boolean isCompositedViewVisible = isCompositedViewVisible();
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, isCompositedViewVisible);
        updateBrowserControlsHeight();
    }

    private int getBrowserControlsHeight() {
        int minHeight = getBrowserControls().getBottomControlsMinHeight();
        int androidViewHeight = getAndroidViewHeight();

        return isCompositedViewVisible() ? androidViewHeight + minHeight : minHeight;
    }

    private int getAndroidViewHeight() {
        int edgeToEdgePadding = 0;

        if (mEdgeToEdgeControllerSupplier.get() != null
                && !EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled()) {
            // TODO(https://crbug.com/327274751): Account for presence of Read Aloud when
            // determining bottom controls height.
            edgeToEdgePadding = mEdgeToEdgePaddingPx;
        }

        return mBottomControlsHeight + edgeToEdgePadding;
    }

    private void updateBrowserControlsHeight() {
        mBottomControlsStacker.setBottomControlsHeight(
                getBrowserControlsHeight(),
                getBrowserControls().getBottomControlsMinHeight(),
                false);
    }

    boolean isCompositedViewVisible() {
        return mIsBottomControlsVisible && !mIsKeyboardVisible && !isInFullscreenMode();
    }

    /**
     * The Android View is the interactive view. The composited view should always be behind the
     * Android view which means we hide the Android view whenever the composited view is hidden. We
     * also hide the Android view as we are scrolling the bottom controls off screen this is done by
     * checking if {@link BrowserControlsStateProvider#getBottomControlOffset()} is non-zero.
     */
    private void updateAndroidViewVisibility() {
        final boolean visible =
                isCompositedViewVisible()
                        && !mIsOverlayPanelShowing
                        && !mIsInSwipeLayout
                        && getBrowserControls().getBottomControlOffset() == 0;
        if (visible) {
            // Translate view so that its bottom is aligned with the "base" y_offset, or the
            // y_offset when the bottom controls aren't offset.
            mModel.set(
                    BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y,
                    mModel.get(BottomControlsProperties.Y_OFFSET));
        }
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, visible);
    }

    @Override
    public void updateObscured(boolean obscureTabContent, boolean obscureToolbar) {
        mModel.set(BottomControlsProperties.IS_OBSCURED, obscureToolbar);
    }

    private BrowserControlsStateProvider getBrowserControls() {
        return mBottomControlsStacker.getBrowserControls();
    }

    // Implements BottomControlsLayer

    @Override
    public int getType() {
        return LayerType.TABSTRIP_TOOLBAR;
    }

    @Override
    public int getHeight() {
        return getAndroidViewHeight();
    }

    @Override
    public @LayerScrollBehavior int getScrollBehavior() {
        return LayerScrollBehavior.ALWAYS_SCROLL_OFF;
    }

    @Override
    public @LayerVisibility int getLayerVisibility() {
        return isCompositedViewVisible() ? LayerVisibility.VISIBLE : LayerVisibility.HIDDEN;
    }

    @Override
    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
        assert BottomControlsStacker.isDispatchingYOffset();
        setYOffset(layerYOffset);
    }

    ChangeObserver getEdgeToEdgeChangeObserverForTesting() {
        return mEdgeToEdgeChangeObserver;
    }

    void simulateEdgeToEdgeChangeForTesting(
            int bottomInset, boolean isDrawingToEdge, boolean isPageOptedIntoEdgeToEdge) {
        onEdgeToEdgeChanged(bottomInset, isDrawingToEdge, isPageOptedIntoEdgeToEdge);
    }
}
