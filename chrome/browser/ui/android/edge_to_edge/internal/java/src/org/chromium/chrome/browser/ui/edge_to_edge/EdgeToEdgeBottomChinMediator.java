// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.DIVIDER_COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HAS_CONSTRAINT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.OFFSET_TAG;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.isBottomChinAllowed;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeSupplier;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class EdgeToEdgeBottomChinMediator
        implements LayoutStateProvider.LayoutStateObserver,
                KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                InsetObserver.WindowInsetObserver,
                EdgeToEdgeSupplier.ChangeObserver,
                FullscreenManager.Observer,
                BottomControlsLayer {
    private static final String TAG = "E2EBottomChin";

    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    private int mEdgeToEdgeBottomInsetDp;
    private int mEdgeToEdgeBottomInsetPx;
    private boolean mIsDrawingToEdge;
    private boolean mIsPagedOptedIntoEdgeToEdge;

    // The offset of the composited view in the browser. When BCIV is enabled, this will usually
    // not be equal to the offset in the renderer, since the composited view will be moved by viz
    // instead of the browser.
    private int mYOffset;

    private int mNavigationBarColor;
    private int mDividerColor;

    /**
     * Tracks the latest value for layer visibility to watch for any changes to communicate to the
     * {@link BottomControlsStacker}.
     */
    private @LayerVisibility int mLatestLayerVisibility;

    private boolean mIsKeyboardVisible;
    private int mKeyboardInset;
    private boolean mHasSafeAreaConstraint;

    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final InsetObserver mInsetObserver;
    private final LayoutManager mLayoutManager;
    private final EdgeToEdgeController mEdgeToEdgeController;
    private final BottomControlsStacker mBottomControlsStacker;
    private final FullscreenManager mFullscreenManager;
    private final boolean mIsConstraintChinScrollableWhenStacking;

    /**
     * Build a new mediator for the bottom chin component.
     *
     * @param model The {@link EdgeToEdgeBottomChinProperties} that holds all the view state for the
     *     bottom chin component.
     * @param keyboardVisibilityDelegate A {@link KeyboardVisibilityDelegate} for watching keyboard
     *     visibility events.
     * @param insetObserver The {@link InsetObserver} for checking IME insets.
     * @param layoutManager The {@link LayoutManager} for observing active layout type.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     * @param fullscreenManager The {@link FullscreenManager} for provide the fullscreen state.
     */
    EdgeToEdgeBottomChinMediator(
            PropertyModel model,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            InsetObserver insetObserver,
            LayoutManager layoutManager,
            EdgeToEdgeController edgeToEdgeController,
            BottomControlsStacker bottomControlsStacker,
            FullscreenManager fullscreenManager) {
        mModel = model;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mInsetObserver = insetObserver;
        mLayoutManager = layoutManager;
        mEdgeToEdgeController = edgeToEdgeController;
        mBottomControlsStacker = bottomControlsStacker;
        mFullscreenManager = fullscreenManager;
        mIsConstraintChinScrollableWhenStacking =
                EdgeToEdgeUtils.isConstraintBottomChinScrollableWhenStacking();

        // Add observers.
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(this);
        mInsetObserver.addObserver(this);
        mLayoutManager.addObserver(this);
        mEdgeToEdgeController.registerObserver(this);
        mBottomControlsStacker.addLayer(this);
        mFullscreenManager.addObserver(this);

        // Call observer methods to trigger initial value.
        mLatestLayerVisibility = getLayerVisibility();
        onToEdgeChange(
                mEdgeToEdgeController.getBottomInsetPx(),
                mEdgeToEdgeController.isDrawingToEdge(),
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
        updateHeightAndVisibility();
    }

    void destroy() {
        assert mKeyboardVisibilityDelegate != null;
        assert mInsetObserver != null;
        assert mLayoutManager != null;
        assert mEdgeToEdgeController != null;
        assert mBottomControlsStacker != null;
        assert mFullscreenManager != null;

        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(this);
        mInsetObserver.removeObserver(this);
        mLayoutManager.removeObserver(this);
        mEdgeToEdgeController.unregisterObserver(this);
        mBottomControlsStacker.removeLayer(this);
        mFullscreenManager.removeObserver(this);
    }

    private boolean isVisible() {
        // This assumes the chin is at the very bottom, or all layers below the chin are scrollable.
        if (ChromeFeatureList.sBcivBottomControls.isEnabled() && mModel.get(OFFSET_TAG) != null) {
            return mBottomControlsStacker.getBrowserControls().getBottomControlOffset()
                    < mModel.get(HEIGHT);
        } else {
            return mYOffset < mModel.get(HEIGHT);
        }
    }

    /** Change the color of the bottom chin. */
    void changeBottomChinColor(int color) {
        mNavigationBarColor = color;
        if (!isVisible()) {
            return;
        }
        mModel.set(COLOR, color);
    }

    /** Change the color of the bottom chin. */
    void changeBottomChinDividerColor(int dividerColor) {
        mDividerColor = dividerColor;
        if (!isVisible()) {
            return;
        }
        mModel.set(DIVIDER_COLOR, dividerColor);
    }

    /**
     * Updates the height and visibility for the bottom chin. If either of these changes, that will
     * affect how the bottom chin interacts with the bottom controls, so a layer update will be
     * requested - unifying height and visibility updates in a single method avoids potential
     * redundant layer update requests.
     */
    private void updateHeightAndVisibility() {
        int newHeight = mEdgeToEdgeBottomInsetPx;
        boolean isKeyboardVisible = mIsKeyboardVisible && mKeyboardInset > 0;
        boolean newVisibility =
                mIsDrawingToEdge
                        && isBottomChinAllowed(
                                mLayoutManager.getActiveLayoutType(), mEdgeToEdgeBottomInsetDp)
                        && !mFullscreenManager.getPersistentFullscreenMode()
                        && !isKeyboardVisible;

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
    public void onSafeAreaConstraintChanged(boolean hasConstraint) {
        if (mHasSafeAreaConstraint == hasConstraint) return;
        // mHasSafeAreaConstraint impacts scroll behavior which changes the min height of browser
        // controls layers. Request an update to refresh the calculated height in the stacker.
        mHasSafeAreaConstraint = hasConstraint;
        mModel.set(HAS_CONSTRAINT, mHasSafeAreaConstraint);
        mBottomControlsStacker.requestLayerUpdate(false);
    }

    // KeyboardVisibilityDelegate.KeyboardVisibilityListener

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        mIsKeyboardVisible = isShowing;
        updateHeightAndVisibility();
    }

    // Inset

    @Override
    public void onKeyboardInsetChanged(int inset) {
        mKeyboardInset = inset;
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
        if (!mHasSafeAreaConstraint
                || (mIsPagedOptedIntoEdgeToEdge && mIsConstraintChinScrollableWhenStacking)) {
            return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
        }
        return LayerScrollBehavior.NEVER_SCROLL_OFF;
    }

    @Override
    public int getHeight() {
        return mModel.get(HEIGHT);
    }

    @Override
    public @LayerVisibility int getLayerVisibility() {
        if (mIsKeyboardVisible && mKeyboardInset > 0) return LayerVisibility.HIDDEN;
        return (mModel.get(CAN_SHOW) && !mIsPagedOptedIntoEdgeToEdge)
                ? LayerVisibility.VISIBLE
                : LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE;
    }

    @Override
    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
        assert BottomControlsStacker.isDispatchingYOffset();

        mYOffset = layerYOffset;

        if (isVisible()) {
            // If the chin isn't visible, cache the color and update it when the chin is visible.
            // This is done to reduce the number of compositor frames submitted while scrolling.
            // The color is unnecessarily set to null when the chin gets scrolled off screen, and
            // gets set back to what it was before it was scrolled off.
            changeBottomChinColor(mNavigationBarColor);
            changeBottomChinDividerColor(mDividerColor);
        }

        mModel.set(Y_OFFSET, layerYOffset);
    }

    @Override
    public int updateOffsetTag(BrowserControlsOffsetTagsInfo offsetTagsInfo) {
        mModel.set(OFFSET_TAG, offsetTagsInfo.getBottomControlsOffsetTag());
        return 0;
    }

    @Override
    public void clearOffsetTag() {
        mModel.set(OFFSET_TAG, null);
    }

    public int getDividerColorForTesting() {
        return mDividerColor;
    }

    public int getNavigationBarColorForTesting() {
        return mNavigationBarColor;
    }
}
