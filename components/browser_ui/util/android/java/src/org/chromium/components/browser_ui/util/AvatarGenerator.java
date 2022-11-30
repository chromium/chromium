// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

/** Utilities for manipulating account avatars. */
public class AvatarGenerator {
    /**
     * Rescales avatar image and crops it into a circle.
     * @param resources the Resources used to set initial target density.
     * @param avatar the uncropped avatar.
     * @param imageSize the target image size in pixels.
     * @return the scaled and cropped avatar.
     */
    public static Drawable makeRoundAvatar(Resources resources, Bitmap avatar, int imageSize) {
        if (avatar == null) return null;

        Bitmap output = Bitmap.createBitmap(imageSize, imageSize, Config.ARGB_8888);
        Canvas canvas = new Canvas(output);
        // Fill the canvas with transparent color.
        canvas.drawColor(Color.TRANSPARENT);
        // Draw a white circle.
        float radius = (float) imageSize / 2;
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setColor(Color.WHITE);
        canvas.drawCircle(radius, radius, radius, paint);
        // Use SRC_IN so white circle acts as a mask while drawing the avatar.
        paint.setXfermode(new PorterDuffXfermode(Mode.SRC_IN));
        canvas.drawBitmap(avatar, null, new Rect(0, 0, imageSize, imageSize), paint);
        return new BitmapDrawable(resources, output);
    }
}
