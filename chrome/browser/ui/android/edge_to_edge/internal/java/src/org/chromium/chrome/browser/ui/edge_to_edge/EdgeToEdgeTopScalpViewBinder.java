// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.OFFSET_TAG;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.Y_OFFSET;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@NullMarked
class EdgeToEdgeTopScalpViewBinder {
    static void bind(
            PropertyModel model, EdgeToEdgeTopScalpSceneLayer sceneLayer, PropertyKey propertyKey) {
        if (Y_OFFSET == propertyKey) {
            sceneLayer.setYOffset(model.get(Y_OFFSET));
        } else if (HEIGHT == propertyKey) {
            sceneLayer.setHeight(model.get(HEIGHT));
        } else if (COLOR == propertyKey) {
            sceneLayer.setColor(model.get(COLOR));
        } else if (OFFSET_TAG == propertyKey) {
            sceneLayer.setOffsetTag(model.get(OFFSET_TAG));
        } else if (IS_VISIBLE == propertyKey) {
            sceneLayer.setIsVisible(model.get(IS_VISIBLE));
        } else {
            assert false : "Unhandled property detected in EdgeToEdgeTopScalpViewBinder!";
        }
    }

    /**
     * No-op binder for {@link CompositorModelChangeProcessor}.
     *
     * <p>SceneLayer properties are bound synchronously in {@link #bind} (via {@link
     * PropertyModelChangeProcessor}) so that {@link
     * EdgeToEdgeTopScalpSceneLayer#isSceneOverlayTreeShowing()} is up-to-date before the next
     * frame's {@code pushUpdate()} runs, while {@code CompositorModelChangeProcessor} is used only
     * to schedule compositor frame updates via {@code LayoutManager} for non-excluded keys,
     * matching {@link EdgeToEdgeBottomChinViewBinder}.
     *
     * <p>No-op because SceneLayer properties are updated directly in #bind (as CompositorMCP skips
     * binding excluded keys like Y_OFFSET and IS_VISIBLE). CompositorMCP is only used here to
     * request new compositor frames when non-excluded properties change.
     */
    static void bindCompositorMCP(
            PropertyModel model,
            EdgeToEdgeTopScalpSceneLayer sceneLayer,
            @Nullable PropertyKey propertyKey) {
        assert propertyKey == null;
    }
}
