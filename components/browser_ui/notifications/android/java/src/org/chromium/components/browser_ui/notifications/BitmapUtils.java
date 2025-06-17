// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.NullMarked;

/** Helper methods for dealing with Bitmaps. */
@NullMarked
public class BitmapUtils {
    public static @NonNull Bitmap resizeBitmap(@NonNull Bitmap bitmap, int desiredSizeInKb) {
        int imageSizeInKb = bitmap.getAllocationByteCount() / 1000;
        double ratio = Math.sqrt((double) desiredSizeInKb / (double) imageSizeInKb);
        int newWidth = (int) (bitmap.getWidth() * ratio);
        int newHeight = (int) (bitmap.getHeight() * ratio);
        return Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, true);
    }
}
