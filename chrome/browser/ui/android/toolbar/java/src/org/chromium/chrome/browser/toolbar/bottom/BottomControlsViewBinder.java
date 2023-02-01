// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;

import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class BottomControlsViewBinder {
    /**
     * A wrapper class that holds a {@link ScrollingBottomViewResourceFrameLayout}
     * and a composited layer to be used with the {@link BottomControlsViewBinder}.
     */
    static class ViewHolder {
        /** A handle to the Android View based version of the bottom controls. */
        public final ScrollingBottomViewResourceFrameLayout root;

        /** A handle to the composited bottom controls layer. */
        public ScrollingBottomViewSceneLayer sceneLayer;

        /**
         * @param bottomControlsRootView The Android View based bottom controls.
         */
        public ViewHolder(ScrollingBottomViewResourceFrameLayout bottomControlsRootView,
                ScrollingBottomViewSceneLayer layer) {
            root = bottomControlsRootView;
            sceneLayer = layer;
        }
    }

    static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (BottomControlsProperties.BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX == propertyKey) {
            View bottomControlsWrapper = view.root.findViewById(R.id.bottom_controls_wrapper);
            bottomControlsWrapper.getLayoutParams().height =
                    model.get(BottomControlsProperties.BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX);
        } else if (BottomControlsProperties.Y_OFFSET == propertyKey) {
            view.sceneLayer.setYOffset(model.get(BottomControlsProperties.Y_OFFSET));
        } else if (BottomControlsProperties.ANDROID_VIEW_VISIBLE == propertyKey
                || BottomControlsProperties.COMPOSITED_VIEW_VISIBLE == propertyKey) {
            final boolean showAndroidView =
                    model.get(BottomControlsProperties.ANDROID_VIEW_VISIBLE);
            final boolean showCompositedView =
                    model.get(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE);
            view.root.setVisibility(showAndroidView ? View.VISIBLE : View.INVISIBLE);
            view.sceneLayer.setIsVisible(showCompositedView);
            if (!showAndroidView && !showCompositedView) {
                view.root.getResourceAdapter().dropCachedBitmap();
            }
        } else if (BottomControlsProperties.IS_OBSCURED == propertyKey) {
            view.root.setImportantForAccessibility(model.get(BottomControlsProperties.IS_OBSCURED)
                            ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        } else {
            assert false : "Unhandled property detected in BottomControlsViewBinder!";
        }
    }

    static void bindCompositorMCP(PropertyModel model, ScrollingBottomViewSceneLayer sceneLayer,
            PropertyKey propertyKey) {
        assert propertyKey == null;
    }
}
