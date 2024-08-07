// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

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
        if (EdgeToEdgeBottomChinProperties.Y_OFFSET == propertyKey) {
            viewHolder.mSceneLayer.setYOffset(model.get(EdgeToEdgeBottomChinProperties.Y_OFFSET));
        } else if (EdgeToEdgeBottomChinProperties.HEIGHT == propertyKey) {
            ViewGroup.LayoutParams lp = viewHolder.mAndroidView.getLayoutParams();
            lp.height = model.get(EdgeToEdgeBottomChinProperties.HEIGHT);
            viewHolder.mAndroidView.setLayoutParams(lp);
            viewHolder.mSceneLayer.setHeight(model.get(EdgeToEdgeBottomChinProperties.HEIGHT));
        } else if (EdgeToEdgeBottomChinProperties.IS_VISIBLE == propertyKey) {
            viewHolder.mAndroidView.setVisibility(
                    model.get(EdgeToEdgeBottomChinProperties.IS_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
            viewHolder.mSceneLayer.setIsVisible(
                    model.get(EdgeToEdgeBottomChinProperties.IS_VISIBLE));
        } else if (EdgeToEdgeBottomChinProperties.COLOR == propertyKey) {
            viewHolder.mSceneLayer.setColor(model.get(EdgeToEdgeBottomChinProperties.COLOR));
        } else if (EdgeToEdgeBottomChinProperties.DIVIDER_COLOR == propertyKey) {
            viewHolder.mSceneLayer.setDividerColor(
                    model.get(EdgeToEdgeBottomChinProperties.DIVIDER_COLOR));
        } else {
            assert false : "Unhandled property detected in EdgeToEdgeBottomChinViewBinder!";
        }
    }

    // TODO(crbug.com/345383907) Move #bind logic to the compositor MCP method
    static void bindCompositorMCP(
            PropertyModel model,
            EdgeToEdgeBottomChinSceneLayer sceneLayer,
            PropertyKey propertyKey) {
        assert propertyKey == null;
    }
}
