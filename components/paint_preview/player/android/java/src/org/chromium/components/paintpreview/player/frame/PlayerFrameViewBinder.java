// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Point;
import android.graphics.Rect;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds property changes in {@link PropertyModel} to {@link PlayerFrameView}. */
class PlayerFrameViewBinder {
    static void bind(PropertyModel model, PlayerFrameView view, PropertyKey key) {
        if (key.equals(PlayerFrameProperties.BITMAP_MATRIX)) {
            view.updateBitmapMatrix(model.get(PlayerFrameProperties.BITMAP_MATRIX));
        } else if (key.equals(PlayerFrameProperties.TILE_DIMENSIONS)) {
            view.updateTileDimensions(model.get(PlayerFrameProperties.TILE_DIMENSIONS));
        } else if (key.equals(PlayerFrameProperties.OFFSET)) {
            Point offset = model.get(PlayerFrameProperties.OFFSET);
            view.updateOffset(offset.x, offset.y);
        } else if (key.equals(PlayerFrameProperties.VIEWPORT)) {
            Rect viewPort = model.get(PlayerFrameProperties.VIEWPORT);
            view.updateViewPort(viewPort.left, viewPort.top, viewPort.right, viewPort.bottom);
        } else if (key.equals(PlayerFrameProperties.SUBFRAME_VIEWS)) {
            view.updateSubFrameViews(model.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        } else if (key.equals(PlayerFrameProperties.SUBFRAME_RECTS)) {
            view.updateSubFrameRects(model.get(PlayerFrameProperties.SUBFRAME_RECTS));
        } else if (key.equals(PlayerFrameProperties.SCALE_MATRIX)) {
            view.updateScaleMatrix(model.get(PlayerFrameProperties.SCALE_MATRIX));
        }
    }
}
