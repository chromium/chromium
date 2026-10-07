// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.Y_OFFSET;

import androidx.annotation.ColorInt;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.ScrollBehavior;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class EdgeToEdgeTopScalpMediator
        implements TopControlLayer,
                Destroyable,
                EdgeToEdgeSupplier.ChangeObserver,
                FullscreenManager.Observer,
                LayoutStateProvider.LayoutStateObserver,
                ThemeColorObserver {

    private final PropertyModel mModel;
    private final TopControlsStacker mTopControlsStacker;
    private final EdgeToEdgeController mEdgeToEdgeController;
    private final LayoutManager mLayoutManager;
    private final FullscreenManager mFullscreenManager;
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;

    /**
     * Creates an {@link EdgeToEdgeTopScalpMediator}.
     *
     * @param model The {@link PropertyModel} for the top scalp.
     * @param topControlsStacker The {@link TopControlsStacker} to register this layer with.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing edge-to-edge
     *     state.
     * @param layoutManager The {@link LayoutManager} for observing layout state changes.
     * @param fullscreenManager The {@link FullscreenManager} for observing fullscreen state.
     * @param topUiThemeColorProvider The {@link TopUiThemeColorProvider} for theme colors.
     */
    EdgeToEdgeTopScalpMediator(
            PropertyModel model,
            TopControlsStacker topControlsStacker,
            EdgeToEdgeController edgeToEdgeController,
            LayoutManager layoutManager,
            FullscreenManager fullscreenManager,
            TopUiThemeColorProvider topUiThemeColorProvider) {
        mModel = model;
        mTopControlsStacker = topControlsStacker;
        mEdgeToEdgeController = edgeToEdgeController;
        mLayoutManager = layoutManager;
        mFullscreenManager = fullscreenManager;
        mTopUiThemeColorProvider = topUiThemeColorProvider;

        mTopControlsStacker.addControl(this);

        // Observe EdgeToEdgeController via ChangeObserver. EdgeToEdgeControllerImpl
        // notifies mEdgeChangeObservers whenever top edge-to-edge state or window insets change.
        mEdgeToEdgeController.registerObserver(this);
        mLayoutManager.addObserver(this);
        mFullscreenManager.addObserver(this);
        mTopUiThemeColorProvider.addThemeColorObserver(this);
        updateColor(mTopUiThemeColorProvider.getThemeColor());
        updateHeightAndVisibility();
    }

    @Override
    public void destroy() {
        mEdgeToEdgeController.unregisterObserver(this);
        mLayoutManager.removeObserver(this);
        mFullscreenManager.removeObserver(this);
        mTopUiThemeColorProvider.removeThemeColorObserver(this);
        mTopControlsStacker.removeControl(this);
    }

    @Override
    public void onToEdgeChange(
            int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
        updateHeightAndVisibility();
    }

    /** Updates the height and visibility of the top scalp based on the current window state. */
    private void updateHeightAndVisibility() {
        // We should check isTopEdgeToEdgeEnabled() rather than isEdgeToEdgeRefactorEnabled()
        // because
        // isEdgeToEdgeRefactorEnabled only unifies edgeless native page
        // inset consumption, it doesn't include creating and using the top scalp UI.
        // Check FullscreenManager directly in addition to EdgeToEdgeController#isDrawingToTopEdge()
        // so entering fullscreen immediately hides the scalp regardless of observer notification
        // order (LayoutManager layout-type checks will be added in a follow up
        // crbug.com/505803562).
        boolean isTopE2eEnabled = EdgeToEdgeUtils.isTopEdgeToEdgeEnabled();
        boolean isFullscreen = mFullscreenManager.getPersistentFullscreenMode();
        int newHeight =
                (isTopE2eEnabled && !isFullscreen && mEdgeToEdgeController.isDrawingToTopEdge())
                        ? mEdgeToEdgeController.getSystemTopInsetPx()
                        : 0;
        boolean newCanShow = newHeight > 0;

        boolean heightChanged = mModel.get(HEIGHT) != newHeight;
        boolean visibilityChanged = mModel.get(CAN_SHOW) != newCanShow;

        if (heightChanged) {
            mModel.set(HEIGHT, newHeight);
        }
        if (visibilityChanged) {
            mModel.set(CAN_SHOW, newCanShow);
        }
        // TODO(crbug.com/505803562): Handle Y_OFFSET updates on height and visibility changes.
        mModel.set(IS_VISIBLE, isVisible());

        if (heightChanged || visibilityChanged) {
            mTopControlsStacker.requestLayerUpdatePost(/* requireAnimate= */ false);
        }
    }

    /** Updates the theme color of the scalp. */
    private void updateColor(@ColorInt int themeColor) {
        // TODO(crbug.com/505803562): Defer COLOR model updates while !isVisible() to avoid
        // unnecessary compositor frame requests when hidden.
        mModel.set(COLOR, themeColor);
    }

    @Override
    public void onThemeColorChanged(@ColorInt int color, boolean shouldAnimate) {
        // TODO(crbug.com/505803562): Support color animation parity with bottom chin / status
        // indicator if shouldAnimate is true, for viewport-fit=auto.
        updateColor(color);
    }

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.TOP_SCALP;
    }

    @Override
    public int getTopControlHeight() {
        return mModel.get(HEIGHT);
    }

    @Override
    public @TopControlVisibility int getTopControlVisibility() {
        return mModel.get(CAN_SHOW) ? TopControlVisibility.VISIBLE : TopControlVisibility.HIDDEN;
    }

    @Override
    public boolean contributesToTotalHeight() {
        return true;
    }

    @Override
    public @ScrollBehavior int getScrollBehavior() {
        // TODO(crbug.com/505803562): Support dynamic scrollability in a follow-up CL.
        return ScrollBehavior.NEVER_SCROLLABLE;
    }

    @Override
    public void onBrowserControlsOffsetUpdate(int layerYOffset, boolean reachRestingPosition) {
        mModel.set(Y_OFFSET, layerYOffset);
        mModel.set(IS_VISIBLE, isVisible());
    }

    @Override
    public void updateOffsetTag(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {
        // TODO(crbug.com/505803562): Implement in follow-up CL when scrollable top scalp is
        // supported.
    }

    @Override
    public void prepForHeightAdjustmentAnimation(int latestYOffset) {
        // TODO(crbug.com/505803562): Implement in follow-up CL when scrollable top scalp is
        // supported.
    }

    private boolean isVisible() {
        return mModel.get(CAN_SHOW) && (mModel.get(Y_OFFSET) + mModel.get(HEIGHT) > 0);
    }

    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        updateHeightAndVisibility();
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        updateHeightAndVisibility();
    }

    @Override
    public void onStartedShowing(@LayoutType int layoutType) {
        // TODO(crbug.com/505803562): Handle layoutType transitions (e.g. Hub) in follow-up CL.
        updateHeightAndVisibility();
    }
}
