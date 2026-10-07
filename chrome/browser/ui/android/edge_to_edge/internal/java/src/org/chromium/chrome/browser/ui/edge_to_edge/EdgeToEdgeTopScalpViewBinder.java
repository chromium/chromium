// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.OFFSET_TAG;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.Y_OFFSET;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@NullMarked
class EdgeToEdgeTopScalpViewBinder {
    static class ViewHolder {
        /**
         * The optional Android view acting as a transparent layout placeholder for the composited
         * top scalp (defined as {@code R.id.edge_to_edge_top_scalp} in {@code main.xml}, wired in a
         * follow-up CL. This view adapts to match the height and vertical translation of the top
         * scalp over the OS status bar inset region, while actual visual rendering is performed by
         * {@link #mSceneLayer}.
         */
        public final @Nullable View mAndroidView;

        /** A handle to the composited edge-to-edge top scalp scene layer. */
        public final EdgeToEdgeTopScalpSceneLayer mSceneLayer;

        public ViewHolder(@Nullable View androidView, EdgeToEdgeTopScalpSceneLayer layer) {
            mAndroidView = androidView;
            mSceneLayer = layer;
        }
    }

    static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (Y_OFFSET == propertyKey) {
            viewHolder.mSceneLayer.setYOffset(model.get(Y_OFFSET));
            if (viewHolder.mAndroidView != null) {
                // Keep the placeholder view aligned with the composited scene layer as top
                // browser controls move (while NEVER_SCROLLABLE, Y_OFFSET rests at 0 when shown).
                viewHolder.mAndroidView.setTranslationY(model.get(Y_OFFSET));
            }
        } else if (HEIGHT == propertyKey) {
            int h = model.get(HEIGHT);
            if (viewHolder.mAndroidView != null) {
                ViewGroup.LayoutParams lp = viewHolder.mAndroidView.getLayoutParams();
                if (lp != null) {
                    lp.height = h;
                    viewHolder.mAndroidView.setLayoutParams(lp);
                }
            }
            viewHolder.mSceneLayer.setHeight(h);
        } else if (CAN_SHOW == propertyKey) {
            updateVisibility(model, viewHolder);
        } else if (COLOR == propertyKey) {
            viewHolder.mSceneLayer.setColor(model.get(COLOR));
        } else if (OFFSET_TAG == propertyKey) {
            viewHolder.mSceneLayer.setOffsetTag(model.get(OFFSET_TAG));
        } else if (IS_VISIBLE == propertyKey) {
            viewHolder.mSceneLayer.setIsVisible(model.get(IS_VISIBLE));
        } else {
            assert false : "Unhandled property detected in EdgeToEdgeTopScalpViewBinder!";
        }
    }

    private static void updateVisibility(PropertyModel model, ViewHolder viewHolder) {
        if (viewHolder.mAndroidView != null) {
            viewHolder.mAndroidView.setVisibility(model.get(CAN_SHOW) ? View.VISIBLE : View.GONE);
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
