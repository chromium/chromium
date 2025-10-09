// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.graphics.Rect;
import android.os.Build;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.display.DisplayUtil;

/** Contains logic for constraints on a {@link ChromeAndroidTask}'s bounds. */
@NullMarked
final class ChromeAndroidTaskBoundsConstraints {
    private static final String TAG = "CATBoundsConstraints";

    private ChromeAndroidTaskBoundsConstraints() {}

    /**
     * Applies all constraints on {@code inputBoundsInPx} and returns the adjusted bounds.
     *
     * @param inputBoundsInPx The input bounds in pixels.
     * @param windowManager The {@link WindowManager} to get the constraints from.
     * @return The adjusted bounds in pixels.
     */
    static Rect apply(Rect inputBoundsInPx, WindowManager windowManager) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "apply() requires Android R+; returning input bounds");
            return inputBoundsInPx;
        }

        Rect maxBounds = getMaxBoundsInPx(windowManager);
        return DisplayUtil.clampRect(inputBoundsInPx, maxBounds);
    }

    /** Returns the maximum bounds in pixels. */
    @RequiresApi(api = Build.VERSION_CODES.R)
    static Rect getMaxBoundsInPx(WindowManager windowManager) {
        var insets =
                windowManager
                        .getMaximumWindowMetrics()
                        .getWindowInsets()
                        .getInsets(WindowInsets.Type.tappableElement());
        Rect fullscreenBounds = windowManager.getMaximumWindowMetrics().getBounds();
        return new Rect(
                0, insets.top, fullscreenBounds.right, fullscreenBounds.bottom - insets.bottom);
    }
}
