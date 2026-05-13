// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.view.WindowManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/**
 * Helper class for Lens Overlay image operations, including compositing the windowshot into a
 * full-screen bitmap.
 */
@NullMarked
public class LensOverlayImageHelper {
    /** Holds screen and window metrics collected on the UI thread for background processing. */
    public static class LensOverlayScreenMetrics {
        public final int screenWidth;
        public final int screenHeight;
        public final Rect windowBounds;

        public LensOverlayScreenMetrics(int screenWidth, int screenHeight, Rect windowBounds) {
            this.screenWidth = screenWidth;
            this.screenHeight = screenHeight;
            this.windowBounds = windowBounds;
        }
    }

    /**
     * Collects the necessary screen and window metrics on the UI thread.
     *
     * @param window The WindowAndroid to get metrics from.
     * @return The collected metrics, or null if context is unavailable.
     */
    public static @Nullable LensOverlayScreenMetrics getScreenMetrics(WindowAndroid window) {
        Context context = window.getContext().get();
        if (context == null) {
            return null;
        }

        WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        if (wm == null) {
            return null;
        }

        DisplayAndroid display = window.getDisplay();
        int screenWidth = display.getDisplayWidth();
        int screenHeight = display.getDisplayHeight();

        Rect windowBounds = new Rect();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            windowBounds = wm.getCurrentWindowMetrics().getBounds();
        } else {
            // Before API 30, we don't have a reliable way to get window-scoped bounds
            // relative to the screen. For the intent-based flow on older devices, we assume
            // the window is full-screen.
            windowBounds.set(0, 0, screenWidth, screenHeight);
        }

        return new LensOverlayScreenMetrics(screenWidth, screenHeight, windowBounds);
    }

    /**
     * Composites a window-scoped bitmap into a full-screen bitmap at the correct offset. If the
     * metrics are invalid or memory allocation fails, the original windowshot is returned.
     *
     * @param metrics The screen and window metrics collected on the UI thread.
     * @param windowshot The bitmap captured from the window.
     * @return A full-screen bitmap with the windowshot inlaid, or the original windowshot.
     */
    public static Bitmap compositeBitmap(LensOverlayScreenMetrics metrics, Bitmap windowshot) {
        try {
            // Create a full-screen black bitmap.
            Bitmap composited =
                    Bitmap.createBitmap(
                            metrics.screenWidth, metrics.screenHeight, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(composited);
            canvas.drawColor(Color.BLACK);

            // Draw the windowshot into the composited bitmap.
            // ui::GrabWindowSnapshot captures the full content of the window, so we draw it
            // at the window's top-left offset.
            canvas.drawBitmap(
                    windowshot, metrics.windowBounds.left, metrics.windowBounds.top, null);

            // Eagerly release the windowshot now that its pixels are copied into the composited
            // bitmap.
            windowshot.recycle();

            return composited;
        } catch (OutOfMemoryError | IllegalArgumentException e) {
            // Fallback: If we can't allocate the full-screen bitmap (due to OOM or extreme
            // dimensions exceeding hardware/mock limits), return the windowshot as-is.
            return windowshot;
        }
    }
}
