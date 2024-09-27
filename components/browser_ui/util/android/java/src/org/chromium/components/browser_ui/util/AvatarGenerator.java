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

import androidx.annotation.Nullable;
import androidx.annotation.Px;

import java.util.List;

/** Utilities for manipulating account avatars. */
public class AvatarGenerator {
    // The margin around every avatar image when multiple are combined together.
    private static final int AVATAR_MARGIN_DIP = 1;

    /**
     * Rescales avatar image and crops it into a circle.
     *
     * @param resources the Resources used to set initial target density.
     * @param avatar the uncropped avatar.
     * @param imageSize the target image size in pixels.
     * @return the scaled and cropped avatar.
     */
    public static @Nullable Drawable makeRoundAvatar(
            Resources resources, Bitmap avatar, @Px int imageSize) {
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

    /**
     * Rescales and combines avatar images and crops the merged image into a circle. If more than 4
     * images are provided, only the first 4 are used to build the avatar.
     *
     * @param resources the Resources used to set initial target density.
     * @param avatars the uncropped avatars.
     * @param imageSize the target image size in pixels.
     * @return the scaled and cropped avatar.
     */
    public static @Nullable Drawable makeRoundAvatar(
            Resources resources, List<Bitmap> avatars, @Px int imageSize) {
        for (Bitmap avatar : avatars) {
            if (avatar == null) return null;
        }
        int avatarCount = avatars.size();
        if (avatarCount == 0) return null;
        if (avatarCount == 1) return makeRoundAvatar(resources, avatars.get(0), imageSize);

        Bitmap output = Bitmap.createBitmap(imageSize, imageSize, Config.ARGB_8888);
        Canvas canvas = new Canvas(output);

        // Each image has a margin of 1 dp around it.
        float margin = AVATAR_MARGIN_DIP * resources.getDisplayMetrics().density;
        float halfSize = imageSize / 2f;

        if (avatarCount == 2) {
            // +------+ +------+
            // |      | |      |
            // |      | |      |
            // |  0   | |  1   |
            // |      | |      |
            // |      | |      |
            // |      | |      |
            // +------+ +------+

            // Left
            canvas.drawBitmap(
                    avatars.get(0),
                    getCenterSliceRect(avatars.get(0)),
                    new Rect(0, 0, (int) (halfSize - margin), imageSize),
                    null);
            // Right
            canvas.drawBitmap(
                    avatars.get(1),
                    getCenterSliceRect(avatars.get(1)),
                    new Rect((int) (halfSize + margin), 0, imageSize, imageSize),
                    null);
        }

        if (avatarCount == 3) {
            // +------+ +------+
            // |      | |  1   |
            // |      | |      |
            // |  0   | +------+
            // |      | +------+
            // |      | |  2   |
            // |      | |      |
            // +------+ +------+

            // Left
            canvas.drawBitmap(
                    avatars.get(0),
                    getCenterSliceRect(avatars.get(0)),
                    new Rect(0, 0, (int) (halfSize - margin), imageSize),
                    null);
            // Top right
            canvas.drawBitmap(
                    avatars.get(1),
                    getFullRect(avatars.get(1)),
                    new Rect((int) (halfSize + margin), 0, imageSize, (int) (halfSize - margin)),
                    null);
            // Bottom right
            canvas.drawBitmap(
                    avatars.get(2),
                    getFullRect(avatars.get(2)),
                    new Rect(
                            (int) (halfSize + margin),
                            (int) (halfSize + margin),
                            imageSize,
                            imageSize),
                    null);
        }

        // Use the first 4 images only.
        if (avatarCount > 3) {
            // +------+ +------+
            // |  0   | |  2   |
            // |      | |      |
            // +------+ +------+
            // +------+ +------+
            // |  1   | |  3   |
            // |      | |      |
            // +------+ +------+

            // Top left
            canvas.drawBitmap(
                    avatars.get(0),
                    getFullRect(avatars.get(0)),
                    new Rect(0, 0, (int) (halfSize - margin), (int) (halfSize - margin)),
                    null);
            // Bottom left
            canvas.drawBitmap(
                    avatars.get(1),
                    getFullRect(avatars.get(1)),
                    new Rect(0, (int) (halfSize + margin), (int) (halfSize - margin), imageSize),
                    null);
            // Top right
            canvas.drawBitmap(
                    avatars.get(2),
                    getFullRect(avatars.get(2)),
                    new Rect((int) (halfSize + margin), 0, imageSize, (int) (halfSize - margin)),
                    null);
            // Bottom right
            canvas.drawBitmap(
                    avatars.get(3),
                    getFullRect(avatars.get(3)),
                    new Rect(
                            (int) (halfSize + margin),
                            (int) (halfSize + margin),
                            imageSize,
                            imageSize),
                    null);
        }
        return makeRoundAvatar(resources, output, imageSize);
    }

    /**
     * Returns the Rect represting the full `avatar`
     *
     * @param avatar the bitmap to which a full rectangle is returned using its full size.
     * @return A Rect that has the same size as the `avatar`
     */
    private static Rect getFullRect(Bitmap avatar) {
        return new Rect(0, 0, avatar.getWidth(), avatar.getHeight());
    }

    /**
     * Returns the Rect of the center slice of `avatar`. The the height is the same as `avatar` and
     * the width is half the width of `avatar` where the center point of `avatar` is also the center
     * point of returned Rect.
     *
     * <pre>
     *  +-------+------------------+--------+
     *  |       |------------------|        |
     *  |       |------------------|        |
     *  |       |--------Rect------|        |
     *  |       |------------------|        |
     *  |       |------------------|        |
     *  +-------+------------------+--------+
     * </pre>
     *
     * @param avatar the bitmap from which the center slice is returned.
     * @return The center slice of `avatar`
     */
    private static Rect getCenterSliceRect(Bitmap avatar) {
        return new Rect(
                (int) (avatar.getWidth() * 0.25),
                0,
                (int) (avatar.getWidth() * 0.75),
                avatar.getHeight());
    }
}
