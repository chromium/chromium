// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;

import org.chromium.base.Callback;

/**
 * Used for communicating with the Paint Preview delegate for requesting new bitmaps and forwarding
 * click events.
 */
public interface PlayerCompositorDelegate {
    /**
     * Requests a new bitmap for a frame from the Paint Preview compositor.
     * @param frameGuid The GUID of the frame.
     * @param clipRect The {@link Rect} for which the bitmap is requested.
     * @param scaleFactor The scale factor at which the bitmap should be rendered.
     * @param bitmapCallback The callback that receives the bitmap once it's ready. Won't get called
     * if there are any errors.
     * @param errorCallback Gets notified if there are any errors. Won't get called otherwise.
     */
    void requestBitmap(long frameGuid, Rect clipRect, float scaleFactor,
            Callback<Bitmap> bitmapCallback, Runnable errorCallback);

    /**
     * Sends a click event for a frame to native for link hit testing.
     * @param frameGuid The GUID of the frame.
     * @param point The coordinates of the click event, relative to the frame.
     */
    void onClick(long frameGuid, Point point);
}
