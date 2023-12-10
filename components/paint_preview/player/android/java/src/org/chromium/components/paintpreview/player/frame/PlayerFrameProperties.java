// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Contains all properties that a player frame {@link PropertyModel} can have. */
class PlayerFrameProperties {
    /** A matrix of bitmap tiles that collectively make the entire content. */
    static final PropertyModel.WritableObjectPropertyKey<Bitmap[][]> BITMAP_MATRIX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    /** The dimensions of each bitmap tile in the current bitmap matrix. */
    static final PropertyModel.WritableObjectPropertyKey<Size> TILE_DIMENSIONS =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** Contains the current user-visible offset. */
    static final PropertyModel.WritableObjectPropertyKey<Point> OFFSET =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    /**
     * Contains the current user-visible content window. The view should use this to draw the
     * appropriate bitmap tiles from {@link #BITMAP_MATRIX}.
     */
    static final PropertyModel.WritableObjectPropertyKey<Rect> VIEWPORT =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    /** List of sub-frame {@link View}s. */
    static final PropertyModel.WritableObjectPropertyKey<List<View>> SUBFRAME_VIEWS =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** List of sub-frame clip rects. */
    static final PropertyModel.WritableObjectPropertyKey<List<Rect>> SUBFRAME_RECTS =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** The matrix to apply to the view before a zoom is committed. */
    static final PropertyModel.WritableObjectPropertyKey<Matrix> SCALE_MATRIX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    static final PropertyKey[] ALL_KEYS = {
        BITMAP_MATRIX,
        TILE_DIMENSIONS,
        OFFSET,
        VIEWPORT,
        SUBFRAME_VIEWS,
        SUBFRAME_RECTS,
        SCALE_MATRIX
    };
}
