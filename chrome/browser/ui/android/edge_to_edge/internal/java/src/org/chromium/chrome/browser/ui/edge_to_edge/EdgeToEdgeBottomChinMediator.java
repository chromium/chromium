// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.DIVIDER_COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.isBottomChinAllowed;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

class EdgeToEdgeBottomChinMediator
        implements LayoutStateProvider.LayoutStateObserver,
                KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                EdgeToEdgeSupplier.ChangeObserver,
                NavigationBarColorProvider.Observer,
                FullscreenManager.Observer,
                BottomControlsLayer {
    private static final String TAG = "E2EBottomChin";

    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    private int mEdgeToEdgeBottomInsetDp;
    private int mEdgeToEdgeBottomInsetPx;
    private boolean mIsDrawingToEdge;
    private boolean mIsPagedOptedIntoEdgeToEdge;

    /**
     * Tracks the latest value for layer visibility to watch for any changes to communicate to the
     * {@link BottomControlsStacker}.
     */
    private @LayerVisibility int mLatestLayerVisibility;

    private boolean mIsKeyboardVisible;

    private final @NonNull KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final @NonNull LayoutManager mLayoutManager;
    private final @NonNull EdgeToEdgeController mEdgeToEdgeController;
    private final @NonNull NavigationBarColorProvider mNavigationBarColorProvider;
    private final @NonNull BottomControlsStacker mBottomControlsStacker;
    private final @NonNull FullscreenManager mFullscreenManager;

    /**
     * Build a new mediator for the bottom chin component.
     *
     * @param model The {@link EdgeToEdgeBottomChinProperties} that holds all the view state for the
     *     bottom chin component.
     * @param keyboardVisibilityDelegate A {@link KeyboardVisibilityDelegate} for watching keyboard
     *     visibility events.
     * @param layoutManager The {@link LayoutManager} for observing active layout type.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param navigationBarColorProvider The {@link NavigationBarColorProvider} for observing the
     *     color for the navigation bar.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     * @param fullscreenManager The {@link FullscreenManager} for provide the fullscreen state.
     */
    EdgeToEdgeBottomChinMediator(
            PropertyModel model,
            @NonNull KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            @NonNull LayoutManager layoutManager,
            @NonNull EdgeToEdgeController edgeToEdgeController,
            @NonNull NavigationBarColorProvider navigationBarColorProvider,
            @NonNull BottomControlsStacker bottomControlsStacker,
            @NonNull FullscreenManager fullscreenManager) {
        mModel = model;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mLayoutManager = layoutManager;
        mEdgeToEdgeController = edgeToEdgeController;
        mNavigationBarColorProvider = navigationBarColorProvider;
        mBottomControlsStacker = bottomControlsStacker;
        mFullscreenManager = fullscreenManager;

        // Add observers.
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(this);
        mLayoutManager.addObserver(this);
        mEdgeToEdgeController.registerObserver(this);
        mNavigationBarColorProvider.addObserver(this);
        mBottomControlsStacker.addLayer(this);
        mFullscreenManager.addObserver(this);

        // Initialize model with appropriate values.
        mModel.set(Y_OFFSET, 0);
        mModel.set(COLOR, mNavigationBarColorProvider.getNavigationBarColor());
        mLatestLayerVisibility = getLayerVisibility();

        // Call observer methods to trigger initial value.
        onToEdgeChange(
                mEdgeToEdgeController.getBottomInsetPx(),
                mEdgeToEdgeController.isDrawingToEdge(),
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
        updateHeightAndVisibility();
    }

    void destroy() {
        assert mKeyboardVisibilityDelegate != null;
        assert mLayoutManager != null;
        assert mEdgeToEdgeController != null;
        assert mNavigationBarColorProvider != null;
        assert mBottomControlsStacker != null;
        assert mFullscreenManager != null;

        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(this);
        mLayoutManager.removeObserver(this);
        mEdgeToEdgeController.unregisterObserver(this);
        mNavigationBarColorProvider.removeObserver(this);
        mBottomControlsStacker.removeLayer(this);
        mFullscreenManager.removeObserver(this);
    }

    /**
     * Updates the height and visibility for the bottom chin. If either of these changes, that will
     * affect how the bottom chin interacts with the bottom controls, so a layer update will be
     * requested - unifying height and visibility updates in a single method avoids potential
     * redundant layer update requests.
     */
    private void updateHeightAndVisibility() {
        int newHeight = mEdgeToEdgeBottomInsetPx;
        boolean newVisibility =
                mIsDrawingToEdge
                        && isBottomChinAllowed(
                                mLayoutManager.getActiveLayoutType(), mEdgeToEdgeBottomInsetDp)
                        && !mFullscreenManager.getPersistentFullscreenMode()
                        && !mIsKeyboardVisible;

        boolean heightChanged = mModel.get(HEIGHT) != newHeight;
        boolean visibilityChanged = mModel.get(CAN_SHOW) != newVisibility;

        if (heightChanged) mModel.set(HEIGHT, newHeight);
        if (visibilityChanged) mModel.set(CAN_SHOW, newVisibility);

        boolean layerVisibilityChanged = mLatestLayerVisibility != getLayerVisibility();
        mLatestLayerVisibility = getLayerVisibility();

        if (heightChanged || visibilityChanged || layerVisibilityChanged) {
            mBottomControlsStacker.requestLayerUpdate(false);
        }
    }

    // LayoutStateProvider.LayoutStateObserver

    @Override
    public void onStartedShowing(int layoutType) {
        updateHeightAndVisibility();
    }

    // EdgeToEdgeSupplier.ChangeObserver

    @Override
    public void onToEdgeChange(
            int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
        if (mEdgeToEdgeBottomInsetDp == bottomInset
                && mIsDrawingToEdge == isDrawingToEdge
                && mIsPagedOptedIntoEdgeToEdge == isPageOptInToEdge) {
            return;
        }

        mEdgeToEdgeBottomInsetDp = bottomInset;
        mEdgeToEdgeBottomInsetPx = mEdgeToEdgeController.getSystemBottomInsetPx();
        mIsDrawingToEdge = isDrawingToEdge;
        mIsPagedOptedIntoEdgeToEdge = isPageOptInToEdge;
        updateHeightAndVisibility();
    }

    @Override
    public void onNavigationBarColorChanged(int color) {
        // TODO(): Animate the color change.
        mModel.set(COLOR, color);
    }

    @Override
    public void onNavigationBarDividerChanged(int dividerColor) {
        mModel.set(DIVIDER_COLOR, dividerColor);
    }

    // KeyboardVisibilityDelegate.KeyboardVisibilityListener

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        mIsKeyboardVisible = isShowing;
        updateHeightAndVisibility();
    }

    // FullscreenManager.Observer

    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        updateHeightAndVisibility();
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        updateHeightAndVisibility();
    }

    // BottomControlsLayer

    @Override
    public int getType() {
        return LayerType.BOTTOM_CHIN;
    }

    @Override
    public int getScrollBehavior() {
        return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
    }

    @Override
    public int getHeight() {
        return mModel.get(HEIGHT);
    }

    @Override
    public @LayerVisibility int getLayerVisibility() {
        return (mModel.get(CAN_SHOW) && !mIsPagedOptedIntoEdgeToEdge)
                ? LayerVisibility.VISIBLE
                : LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE;
    }

    @Override
    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
        assert BottomControlsStacker.isDispatchingYOffset();
        mModel.set(Y_OFFSET, layerYOffset);
    }
}
