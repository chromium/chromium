// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.DIVIDER_COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class EdgeToEdgeBottomChinViewBinder {
    static class ViewHolder {
        /**
         * The Android view acting as a placeholder for the composited bottom chin. This Android
         * view adapts to match the same height as the bottom chin, to extend over the default
         * bottom chin area / OS navbar area. Note that this view is always transparent, and is thus
         * never actually shown. It exists purely as a placeholder to fix some odd bugs where other
         * Android views don't properly draw into the bottom chin space / get cut-off when trying to
         * cover or follow the bottom chin as it scrolls. See crbug.com/356919563.
         */
        public final View mAndroidView;

        /** A handle to the composited edge-to-edge bottom chin scene layer. */
        public EdgeToEdgeBottomChinSceneLayer mSceneLayer;

        /**
         * @param androidView The Android view acting as a placeholder for the composited bottom
         *     chin.
         * @param layer The composited edge-to-edge bottom chin scene layer.
         */
        public ViewHolder(View androidView, EdgeToEdgeBottomChinSceneLayer layer) {
            mAndroidView = androidView;
            mSceneLayer = layer;
        }
    }

    static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (Y_OFFSET == propertyKey) {
            viewHolder.mSceneLayer.setYOffset(model.get(Y_OFFSET));
            updateVisibility(model, viewHolder);
        } else if (HEIGHT == propertyKey) {
            ViewGroup.LayoutParams lp = viewHolder.mAndroidView.getLayoutParams();
            lp.height = model.get(HEIGHT);
            viewHolder.mAndroidView.setLayoutParams(lp);
            viewHolder.mSceneLayer.setHeight(model.get(HEIGHT));
            updateVisibility(model, viewHolder);
        } else if (CAN_SHOW == propertyKey) {
            updateVisibility(model, viewHolder);
        } else if (COLOR == propertyKey) {
            viewHolder.mSceneLayer.setColor(model.get(COLOR));
        } else if (DIVIDER_COLOR == propertyKey) {
            viewHolder.mSceneLayer.setDividerColor(model.get(DIVIDER_COLOR));
        } else {
            assert false : "Unhandled property detected in EdgeToEdgeBottomChinViewBinder!";
        }
    }

    private static void updateVisibility(PropertyModel model, ViewHolder viewHolder) {
        boolean visible = model.get(CAN_SHOW) && model.get(Y_OFFSET) < model.get(HEIGHT);

        viewHolder.mAndroidView.setVisibility(visible ? View.VISIBLE : View.GONE);
        viewHolder.mSceneLayer.setIsVisible(visible);
    }

    // TODO(crbug.com/345383907) Move #bind logic to the compositor MCP method
    static void bindCompositorMCP(
            PropertyModel model,
            EdgeToEdgeBottomChinSceneLayer sceneLayer,
            PropertyKey propertyKey) {
        assert propertyKey == null;
    }
}
