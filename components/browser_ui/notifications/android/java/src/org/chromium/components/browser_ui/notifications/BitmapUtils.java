// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;

/** Helper methods for dealing with Bitmaps. */
@NullMarked
public class BitmapUtils {
    /**
     * Resizes the bitmap to fit within the max dimensions (in dp), and then further reduces it if
     * the resulting bitmap still exceeds the maximum memory limit.
     *
     * @param context The application context used to determine display density.
     * @param bitmap The source bitmap.
     * @param maxWidthDp The maximum allowed width in dp.
     * @param maxHeightDp The maximum allowed height in dp.
     * @param maxKBytes The maximum allowed memory size in kilobytes.
     * @return The resized bitmap, or the original if no resizing was needed.
     */
    public static Bitmap resizeBitmapIfNeeded(
            Context context, Bitmap bitmap, int maxWidthDp, int maxHeightDp, int maxKBytes) {
        float density = context.getResources().getDisplayMetrics().density;
        int maxWidth = Math.round(maxWidthDp * density);
        int maxHeight = Math.round(maxHeightDp * density);
        bitmap = resizeBitmapByDimensions(bitmap, maxWidth, maxHeight);
        return resizeBitmapByMemory(bitmap, maxKBytes);
    }

    /**
     * Resizes the bitmap to fit within the max physical dimensions while preserving aspect ratio.
     *
     * @param bitmap The source bitmap.
     * @param maxWidth The maximum allowed width in pixels.
     * @param maxHeight The maximum allowed height in pixels.
     * @return The resized bitmap, or the original if within the dimensions.
     */
    public static Bitmap resizeBitmapByDimensions(Bitmap bitmap, int maxWidth, int maxHeight) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();

        if (width <= maxWidth && height <= maxHeight) {
            return bitmap;
        }

        double scale = Math.min((double) maxWidth / width, (double) maxHeight / height);
        int newWidth = (int) (width * scale);
        int newHeight = (int) (height * scale);

        return Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, true);
    }

    /**
     * Resizes the bitmap to fit within the desired memory size in kilobytes.
     *
     * @param bitmap The source bitmap.
     * @param desiredSizeInKb The maximum desired memory size in kilobytes.
     * @return The resized bitmap, or the original if within the memory limit.
     */
    public static Bitmap resizeBitmapByMemory(Bitmap bitmap, int desiredSizeInKb) {
        int imageSizeInKb = bitmap.getAllocationByteCount() / 1000;
        if (imageSizeInKb <= desiredSizeInKb) {
            return bitmap;
        }

        double ratio = Math.sqrt((double) desiredSizeInKb / (double) imageSizeInKb);
        int newWidth = (int) (bitmap.getWidth() * ratio);
        int newHeight = (int) (bitmap.getHeight() * ratio);

        return Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, true);
    }
}
