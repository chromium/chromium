// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.graphics.Color;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Set;

/**
 * Coordinator for the Edge-to-Edge Top Scalp UI component.
 *
 * <p>When edgeless top inset is enabled, this component manages the top inset (status bar) area in
 * top browser controls using a dedicated CC SceneLayer and PropertyModel.
 */
@NullMarked
public class EdgeToEdgeTopScalpCoordinator implements Destroyable {
    private final EdgeToEdgeTopScalpMediator mMediator;
    private final EdgeToEdgeTopScalpSceneLayer mSceneLayer;
    private final PropertyModelChangeProcessor<
                    PropertyModel, EdgeToEdgeTopScalpSceneLayer, PropertyKey>
            mModelChangeProcessor;
    private final CompositorModelChangeProcessor<EdgeToEdgeTopScalpSceneLayer> mCompositorMCP;

    /**
     * Creates an {@link EdgeToEdgeTopScalpCoordinator} component.
     *
     * @param layoutManager The {@link LayoutManager} for adding new scene overlays.
     * @param topControlsStacker The {@link TopControlsStacker} to register this layer with.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for edge-to-edge state.
     * @param fullscreenManager The {@link FullscreenManager} providing fullscreen state.
     * @param topUiThemeColorProvider The {@link TopUiThemeColorProvider} providing theme colors.
     */
    public EdgeToEdgeTopScalpCoordinator(
            LayoutManager layoutManager,
            TopControlsStacker topControlsStacker,
            EdgeToEdgeController edgeToEdgeController,
            FullscreenManager fullscreenManager,
            TopUiThemeColorProvider topUiThemeColorProvider) {
        this(
                layoutManager,
                topControlsStacker,
                edgeToEdgeController,
                // Pass null for requestRenderRunnable because mCompositorMCP requests a
                // compositor frame whenever any scene-layer property changes.
                new EdgeToEdgeTopScalpSceneLayer(/* requestRenderRunnable= */ null),
                fullscreenManager,
                topUiThemeColorProvider);
    }

    @VisibleForTesting
    EdgeToEdgeTopScalpCoordinator(
            LayoutManager layoutManager,
            TopControlsStacker topControlsStacker,
            EdgeToEdgeController edgeToEdgeController,
            EdgeToEdgeTopScalpSceneLayer sceneLayer,
            FullscreenManager fullscreenManager,
            TopUiThemeColorProvider topUiThemeColorProvider) {
        mSceneLayer = sceneLayer;

        PropertyModel model =
                new PropertyModel.Builder(EdgeToEdgeTopScalpProperties.ALL_KEYS)
                        .with(EdgeToEdgeTopScalpProperties.HEIGHT, 0)
                        .with(EdgeToEdgeTopScalpProperties.COLOR, Color.TRANSPARENT)
                        .with(EdgeToEdgeTopScalpProperties.Y_OFFSET, 0)
                        .with(EdgeToEdgeTopScalpProperties.IS_VISIBLE, false)
                        .build();

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, sceneLayer, EdgeToEdgeTopScalpViewBinder::bind);

        Set<PropertyKey> exclusions =
                Set.of(
                        EdgeToEdgeTopScalpProperties.Y_OFFSET,
                        EdgeToEdgeTopScalpProperties.OFFSET_TAG,
                        EdgeToEdgeTopScalpProperties.IS_VISIBLE);
        mCompositorMCP =
                layoutManager.createCompositorMCPWithExclusions(
                        model,
                        sceneLayer,
                        EdgeToEdgeTopScalpViewBinder::bindCompositorMCP,
                        exclusions);
        layoutManager.addSceneOverlay(sceneLayer);

        mMediator =
                new EdgeToEdgeTopScalpMediator(
                        model,
                        topControlsStacker,
                        edgeToEdgeController,
                        layoutManager,
                        fullscreenManager,
                        topUiThemeColorProvider);
    }

    @Override
    public void destroy() {
        mCompositorMCP.destroy();
        mModelChangeProcessor.destroy();
        mMediator.destroy();
        mSceneLayer.destroy();
    }
}
