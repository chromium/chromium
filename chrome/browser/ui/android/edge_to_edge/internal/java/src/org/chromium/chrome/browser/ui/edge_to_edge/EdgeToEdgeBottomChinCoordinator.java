// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The bottom chin is a compositor layer that visually imitates the device's bottom OS navbar when
 * showing tab content, but can be scrolled off as it is a part of the browser controls. This allows
 * for an edge-to-edge like experience, with the ability to scroll back the bottom chin / "OS
 * navbar" to better view and access bottom-anchored web content.
 */
public class EdgeToEdgeBottomChinCoordinator implements Destroyable {
    private final EdgeToEdgeBottomChinMediator mMediator;
    private final LayoutManager mLayoutManager;
    private final EdgeToEdgeBottomChinSceneLayer mSceneLayer;

    /**
     * Build the coordinator that manages the edge-to-edge bottom chin.
     *
     * @param layoutManager The {@link LayoutManager} for adding new scene overlays.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param navigationBarColorProvider The {@link NavigationBarColorProvider} for observing the
     *     color for the navigation bar.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     */
    public EdgeToEdgeBottomChinCoordinator(
            @NonNull LayoutManager layoutManager,
            @NonNull EdgeToEdgeController edgeToEdgeController,
            @NonNull NavigationBarColorProvider navigationBarColorProvider,
            @NonNull BottomControlsStacker bottomControlsStacker) {
        this(
                layoutManager,
                edgeToEdgeController,
                navigationBarColorProvider,
                bottomControlsStacker,
                new EdgeToEdgeBottomChinSceneLayer());
    }

    @VisibleForTesting
    EdgeToEdgeBottomChinCoordinator(
            @NonNull LayoutManager layoutManager,
            @NonNull EdgeToEdgeController edgeToEdgeController,
            @NonNull NavigationBarColorProvider navigationBarColorProvider,
            @NonNull BottomControlsStacker bottomControlsStacker,
            @NonNull EdgeToEdgeBottomChinSceneLayer sceneLayer) {
        mLayoutManager = layoutManager;
        mSceneLayer = sceneLayer;

        PropertyModel model =
                new PropertyModel.Builder(EdgeToEdgeBottomChinProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                model, sceneLayer, EdgeToEdgeBottomChinViewBinder::bind);
        mLayoutManager.createCompositorMCP(
                model, sceneLayer, EdgeToEdgeBottomChinViewBinder::bindCompositorMCP);

        mMediator =
                new EdgeToEdgeBottomChinMediator(
                        model,
                        mLayoutManager,
                        edgeToEdgeController,
                        navigationBarColorProvider,
                        bottomControlsStacker);

        mLayoutManager.addSceneOverlay(sceneLayer);
    }

    /** Clean up all observers and release any held resources. */
    @Override
    public void destroy() {
        mMediator.destroy();
        mSceneLayer.destroy();
    }
}
