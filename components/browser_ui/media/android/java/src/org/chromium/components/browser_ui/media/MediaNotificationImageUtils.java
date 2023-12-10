// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;

import androidx.annotation.Nullable;

import org.chromium.base.SysUtils;

/** A collection of utilities and constants for the images used in MediaSession notifications. */
public class MediaNotificationImageUtils {
    public static final int MINIMAL_MEDIA_IMAGE_SIZE_PX = 114;

    // The media artwork image resolution on high-end devices.
    private static final int HIGH_IMAGE_SIZE_PX = 512;

    // The media artwork image resolution on high-end devices.
    private static final int LOW_IMAGE_SIZE_PX = 256;

    /**
     * Downscale |icon| for display in the notification if needed. Returns null if |icon| is null.
     * If |icon| is larger than {@link getIdealMediaImageSize()}, scale it down to
     * {@link getIdealMediaImageSize()} and return. Otherwise return the original |icon|.
     * @param icon The icon to be scaled.
     */
    @Nullable
    public static Bitmap downscaleIconToIdealSize(@Nullable Bitmap icon) {
        if (icon == null) return null;

        int targetSize = getIdealMediaImageSize();

        Matrix m = new Matrix();
        int dominantLength = Math.max(icon.getWidth(), icon.getHeight());

        if (dominantLength < getIdealMediaImageSize()) return icon;

        // Move the center to (0,0).
        m.postTranslate(icon.getWidth() / -2.0f, icon.getHeight() / -2.0f);
        // Scale to desired size.
        float scale = 1.0f * targetSize / dominantLength;
        m.postScale(scale, scale);
        // Move to the desired place.
        m.postTranslate(targetSize / 2.0f, targetSize / 2.0f);

        // Draw the image.
        Bitmap paddedBitmap = Bitmap.createBitmap(targetSize, targetSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(paddedBitmap);
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(icon, m, paint);
        return paddedBitmap;
    }

    /** @return The ideal size of the media image. */
    public static int getIdealMediaImageSize() {
        return SysUtils.isLowEndDevice() ? LOW_IMAGE_SIZE_PX : HIGH_IMAGE_SIZE_PX;
    }

    /**
     * @param icon The icon to be checked.
     * @return Whether |icon| is suitable as the media image, i.e. bigger than the minimal size.
     */
    public static boolean isBitmapSuitableAsMediaImage(Bitmap icon) {
        return icon != null
                && icon.getWidth() >= MINIMAL_MEDIA_IMAGE_SIZE_PX
                && icon.getHeight() >= MINIMAL_MEDIA_IMAGE_SIZE_PX;
    }
}
