// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import android.graphics.Color;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.components.browser_ui.edge_to_edge.SystemBarColorHelper;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The bottom chin is a compositor layer that visually imitates the device's bottom OS navbar when
 * showing tab content, but can be scrolled off as it is a part of the browser controls. This allows
 * for an edge-to-edge like experience, with the ability to scroll back the bottom chin / "OS
 * navbar" to better view and access bottom-anchored web content.
 */
@NullMarked
public class EdgeToEdgeBottomChinCoordinator implements Destroyable, SystemBarColorHelper {
    private final EdgeToEdgeBottomChinMediator mMediator;
    private final LayoutManager mLayoutManager;
    private final EdgeToEdgeBottomChinSceneLayer mSceneLayer;

    /**
     * Build the coordinator that manages the edge-to-edge bottom chin.
     *
     * @param androidView The Android view for the bottom chin.
     * @param keyboardVisibilityDelegate A {@link KeyboardVisibilityDelegate} for watching keyboard
     *     visibility events.
     * @param insetObserver The {@link InsetObserver} for checking IME insets.
     * @param layoutManager The {@link LayoutManager} for adding new scene overlays.
     * @param requestRenderRunnable Runnable that requests a re-render of the scene overlay.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     * @param fullscreenManager The {@link FullscreenManager} for provide the fullscreen state.
     */
    public EdgeToEdgeBottomChinCoordinator(
            View androidView,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            InsetObserver insetObserver,
            LayoutManager layoutManager,
            Runnable requestRenderRunnable,
            EdgeToEdgeController edgeToEdgeController,
            BottomControlsStacker bottomControlsStacker,
            FullscreenManager fullscreenManager) {
        this(
                androidView,
                keyboardVisibilityDelegate,
                insetObserver,
                layoutManager,
                edgeToEdgeController,
                bottomControlsStacker,
                new EdgeToEdgeBottomChinSceneLayer(requestRenderRunnable),
                fullscreenManager);
    }

    @VisibleForTesting
    EdgeToEdgeBottomChinCoordinator(
            View androidView,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            InsetObserver insetObserver,
            LayoutManager layoutManager,
            EdgeToEdgeController edgeToEdgeController,
            BottomControlsStacker bottomControlsStacker,
            EdgeToEdgeBottomChinSceneLayer sceneLayer,
            FullscreenManager fullscreenManager) {
        mLayoutManager = layoutManager;
        mSceneLayer = sceneLayer;

        PropertyModel model =
                new PropertyModel.Builder(EdgeToEdgeBottomChinProperties.ALL_KEYS)
                        .with(EdgeToEdgeBottomChinProperties.CAN_SHOW, false)
                        .with(EdgeToEdgeBottomChinProperties.COLOR, Color.TRANSPARENT)
                        .with(EdgeToEdgeBottomChinProperties.DIVIDER_COLOR, Color.TRANSPARENT)
                        .with(EdgeToEdgeBottomChinProperties.Y_OFFSET, 0)
                        .build();
        PropertyModelChangeProcessor.create(
                model,
                new EdgeToEdgeBottomChinViewBinder.ViewHolder(androidView, sceneLayer),
                EdgeToEdgeBottomChinViewBinder::bind);
        mLayoutManager.createCompositorMCP(
                model, sceneLayer, EdgeToEdgeBottomChinViewBinder::bindCompositorMCP);

        mMediator =
                new EdgeToEdgeBottomChinMediator(
                        model,
                        keyboardVisibilityDelegate,
                        insetObserver,
                        mLayoutManager,
                        edgeToEdgeController,
                        bottomControlsStacker,
                        fullscreenManager);

        mLayoutManager.addSceneOverlay(sceneLayer);
    }

    /** Clean up all observers and release any held resources. */
    @Override
    public void destroy() {
        mMediator.destroy();
        mSceneLayer.destroy();
    }

    // SystemBarColorHelper

    @Override
    public boolean canSetStatusBarColor() {
        return false;
    }

    @Override
    public void setStatusBarColor(int color) {}

    @Override
    public void setNavigationBarColor(int color) {
        mMediator.changeBottomChinColor(color);
    }

    @Override
    public void setNavigationBarDividerColor(int dividerColor) {
        mMediator.changeBottomChinDividerColor(dividerColor);
    }
}
