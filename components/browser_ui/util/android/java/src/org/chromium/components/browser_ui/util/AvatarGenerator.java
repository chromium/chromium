// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Utilities for manipulating account avatars. */
@NullMarked
public class AvatarGenerator {
    // The margin around every avatar image when multiple are combined together.
    private static final int AVATAR_MARGIN_DIP = 1;

    /** Scaling factor for rendering supersampled avatar bitmaps to improve sharpness. */
    @VisibleForTesting static final int SUPERSAMPLING_FACTOR = 2;

    /**
     * Rescales avatar image and crops it into a circle.
     *
     * @param resources the Resources used to set initial target density.
     * @param avatar the uncropped avatar.
     * @param imageSize the target image size in pixels.
     * @return the scaled and cropped avatar.
     */
    @Contract("_, !null, _ -> !null")
    public static @Nullable RoundedBitmapDrawable makeRoundAvatar(
            Resources resources, Bitmap avatar, @Px int imageSize) {
        if (avatar == null) return null;

        // Render at a higher resolution and scale the density accordingly so the returned
        // Drawable retains 1x intrinsic dimensions while providing supersampled resolution.
        // This prevents blurring and resampling artifacts caused by subpixel centering and texture
        // capture passes.
        int renderSize = imageSize * SUPERSAMPLING_FACTOR;
        Bitmap scaledAvatar = Bitmap.createScaledBitmap(avatar, renderSize, renderSize, true);
        if (scaledAvatar == avatar) {
            // Copy if returned as-is by createScaledBitmap to avoid mutating shared cache density.
            scaledAvatar = avatar.copy(Bitmap.Config.ARGB_8888, true);
        }
        scaledAvatar.setDensity(resources.getDisplayMetrics().densityDpi * SUPERSAMPLING_FACTOR);

        RoundedBitmapDrawable roundedAvatar =
                RoundedBitmapDrawableFactory.create(resources, scaledAvatar);
        roundedAvatar.setAntiAlias(true);
        roundedAvatar.setCircular(true);
        return roundedAvatar;
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
    public static @Nullable RoundedBitmapDrawable makeRoundAvatar(
            Resources resources, List<Bitmap> avatars, @Px int imageSize) {
        for (Bitmap avatar : avatars) {
            if (avatar == null) return null;
        }
        int avatarCount = avatars.size();
        if (avatarCount == 0) return null;
        if (avatarCount == 1) return makeRoundAvatar(resources, avatars.get(0), imageSize);

        // Render at supersampled resolution so collage slice details and margins remain sharp.
        int renderSize = imageSize * SUPERSAMPLING_FACTOR;
        Bitmap output = Bitmap.createBitmap(renderSize, renderSize, Config.ARGB_8888);
        Canvas canvas = new Canvas(output);

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);

        // Each image has a margin of 1 dp around it, scaled for supersampled render resolution.
        float margin =
                AVATAR_MARGIN_DIP * resources.getDisplayMetrics().density * SUPERSAMPLING_FACTOR;
        float halfSize = renderSize / 2f;

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
                    new Rect(0, 0, (int) (halfSize - margin), renderSize),
                    paint);
            // Right
            canvas.drawBitmap(
                    avatars.get(1),
                    getCenterSliceRect(avatars.get(1)),
                    new Rect((int) (halfSize + margin), 0, renderSize, renderSize),
                    paint);
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
                    new Rect(0, 0, (int) (halfSize - margin), renderSize),
                    paint);
            // Top right
            canvas.drawBitmap(
                    avatars.get(1),
                    getFullRect(avatars.get(1)),
                    new Rect((int) (halfSize + margin), 0, renderSize, (int) (halfSize - margin)),
                    paint);
            // Bottom right
            canvas.drawBitmap(
                    avatars.get(2),
                    getFullRect(avatars.get(2)),
                    new Rect(
                            (int) (halfSize + margin),
                            (int) (halfSize + margin),
                            renderSize,
                            renderSize),
                    paint);
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
                    paint);
            // Bottom left
            canvas.drawBitmap(
                    avatars.get(1),
                    getFullRect(avatars.get(1)),
                    new Rect(0, (int) (halfSize + margin), (int) (halfSize - margin), renderSize),
                    paint);
            // Top right
            canvas.drawBitmap(
                    avatars.get(2),
                    getFullRect(avatars.get(2)),
                    new Rect((int) (halfSize + margin), 0, renderSize, (int) (halfSize - margin)),
                    paint);
            // Bottom right
            canvas.drawBitmap(
                    avatars.get(3),
                    getFullRect(avatars.get(3)),
                    new Rect(
                            (int) (halfSize + margin),
                            (int) (halfSize + margin),
                            renderSize,
                            renderSize),
                    paint);
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
