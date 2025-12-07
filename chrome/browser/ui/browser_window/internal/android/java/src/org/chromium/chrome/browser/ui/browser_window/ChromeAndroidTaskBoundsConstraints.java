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
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/** Contains logic for constraints on a {@link ChromeAndroidTask}'s bounds. */
@NullMarked
final class ChromeAndroidTaskBoundsConstraints {
    private static final String TAG = "CATBoundsConstraints";

    /**
     * The minimal size of a task, for both width and height.
     *
     * <p>The Android framework defines the minimal size for framework APIs [1], so the minimal size
     * here must be no smaller than that.
     *
     * <p>Note: Apps don't have access to the Android framework's minimal size as it's not public.
     *
     * <p>[1] <a
     * href="https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/res/res/values/dimens.xml;l=792;drc=37507ae292eef969c507d9438a4692539035f764">Link
     * to Android framework's minimal size definition</a>
     */
    static final int MINIMAL_TASK_SIZE_DP = 220;

    private ChromeAndroidTaskBoundsConstraints() {}

    /**
     * Applies all constraints on {@code inputBoundsInPx} and returns the adjusted bounds.
     *
     * @param inputBoundsInPx The input bounds in pixels.
     * @param displayAndroid The display the {@code inputBoundsInPx} is for.
     * @param windowManager The {@link WindowManager} bound to the underlying display of the
     *     provided {@code displayAndroid}.
     * @return The adjusted bounds in pixels.
     */
    static Rect apply(
            Rect inputBoundsInPx, DisplayAndroid displayAndroid, WindowManager windowManager) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.w(TAG, "apply() requires Android R+; returning input bounds");
            return inputBoundsInPx;
        }

        // 1. Get the max and min sizes.
        Rect maxBoundsInPx = getMaxBoundsInPx(windowManager);
        int minWidthAndHeightInPx = DisplayUtil.dpToPx(displayAndroid, MINIMAL_TASK_SIZE_DP);

        // 2. Clamp the input bounds so that it's fully within the max bounds.
        Rect boundsWithinMaxBoundsInPx = DisplayUtil.clampRect(inputBoundsInPx, maxBoundsInPx);
        int adjustedLeftInPx = boundsWithinMaxBoundsInPx.left;
        int adjustedTopInPx = boundsWithinMaxBoundsInPx.top;

        // 3. Make sure the bounds are no smaller than the min size.
        int adjustedWidthInPx = Math.max(boundsWithinMaxBoundsInPx.width(), minWidthAndHeightInPx);
        int adjustedHeightInPx =
                Math.max(boundsWithinMaxBoundsInPx.height(), minWidthAndHeightInPx);

        // 4. Return the adjusted bounds.
        return new Rect(
                adjustedLeftInPx,
                adjustedTopInPx,
                /* right= */ adjustedLeftInPx + adjustedWidthInPx,
                /* bottom= */ adjustedTopInPx + adjustedHeightInPx);
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
