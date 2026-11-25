// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.util.SparseIntArray;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Picks the dominant color of a bitmap captured from the top of the web page. */
@NullMarked
public class TopPageColorObserver {
    // Maximum squared Euclidean RGB distance to group similar colors together
    // (equivalent to roughly +/- 10 per R, G, B channel: 10^2 + 10^2 + 10^2 = 300).
    private static final int MAX_COLOR_DISTANCE_SQUARED = 300;

    // TODO(crbug.com/546588788): Capture the top strip of the page with
    // RenderWidgetHostView#copyFromSurface and pass it to getDominantColor().

    /**
     * Returns the opaque color with the most nearby pixels (within {@link
     * #MAX_COLOR_DISTANCE_SQUARED} in RGB space), or {@code null} if that cluster covers less than
     * half of the bitmap.
     */
    @VisibleForTesting
    static @Nullable @ColorInt Integer getDominantColor(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);

        SparseIntArray colorCounts = new SparseIntArray();
        for (@ColorInt int pixel : pixels) {
            if (Color.alpha(pixel) != 0xFF) continue;
            colorCounts.put(pixel, colorCounts.get(pixel, 0) + 1);
        }
        if (colorCounts.size() == 0) return null;

        @ColorInt int dominantColor = Color.TRANSPARENT;
        int maxBallCount = 0;
        int maxCenterCount = 0;

        int uniqueColorCount = colorCounts.size();
        for (int i = 0; i < uniqueColorCount; i++) {
            @ColorInt int centerColor = colorCounts.keyAt(i);
            int centerCount = colorCounts.valueAt(i);
            int ballCount = 0;

            for (int j = 0; j < uniqueColorCount; j++) {
                @ColorInt int otherColor = colorCounts.keyAt(j);
                if (getColorDistanceSquared(centerColor, otherColor)
                        <= MAX_COLOR_DISTANCE_SQUARED) {
                    ballCount += colorCounts.valueAt(j);
                }
            }

            if (ballCount > maxBallCount
                    || (ballCount == maxBallCount && centerCount > maxCenterCount)) {
                maxBallCount = ballCount;
                maxCenterCount = centerCount;
                dominantColor = centerColor;
            }
        }

        if (maxBallCount * 2 < pixels.length) {
            return null;
        }
        return dominantColor;
    }

    private static int getColorDistanceSquared(@ColorInt int c1, @ColorInt int c2) {
        int dr = Color.red(c1) - Color.red(c2);
        int dg = Color.green(c1) - Color.green(c2);
        int db = Color.blue(c1) - Color.blue(c2);
        return dr * dr + dg * dg + db * db;
    }
}
