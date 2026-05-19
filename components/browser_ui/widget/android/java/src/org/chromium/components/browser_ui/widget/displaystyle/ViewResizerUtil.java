// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.displaystyle;

import static org.chromium.components.browser_ui.widget.displaystyle.UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.display.DisplayUtil;

/** Util class for @{@link org.chromium.components.browser_ui.widget.displaystyle.ViewResizer}. */
@NullMarked
public class ViewResizerUtil {

    /**
     * @see ViewResizerUtil#computePaddingForWideDisplay
     */
    public static int computePaddingForWideDisplay(
            Context context, @Nullable View view, int minWidePaddingPixels) {
        return computePaddingForWideDisplay(context.getResources(), view, minWidePaddingPixels);
    }

    /**
     * When layout has a wide window style, compute padding to constrain to {@link
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}.
     *
     * @param resources The {@link Resources} used to retrieve configuration and display metrics.
     * @param view The {@link View} whose measured width will be used if layout depends on the
     *     container width.
     * @param wideWindowMinPaddingPx The minimum padding needed on wide windows in pixels.
     */
    public static int computePaddingForWideDisplay(
            Resources resources, @Nullable View view, int wideWindowMinPaddingPx) {
        int containerWidthDp = 0;
        // Check the view directly first, as the screen width may be misleading. Some automotive
        // devices with wide displays show side UI components, which should not be included in the
        // padding calculations. Also, on floating windows, screen width is unlikely to be a good
        // proxy for window or view width.
        if (DisplayUtil.isUiScaled() && view != null) {
            containerWidthDp =
                    ViewUtils.pxToDp(resources.getDisplayMetrics(), view.getMeasuredWidth());
        }
        // TODO(crbug.com/514439044): Clean up direct use of screen width if possible.
        // Use the screen width as a fallback if the container width is inapplicable, or unavailable
        // if checked very early on during initialization.
        if (containerWidthDp == 0) {
            containerWidthDp = resources.getConfiguration().screenWidthDp;
        }

        int excessWidthDp = containerWidthDp - WIDE_DISPLAY_STYLE_MIN_WIDTH_DP;
        int paddingPx = ViewUtils.dpToPx(resources.getDisplayMetrics(), excessWidthDp / 2.f);
        return Math.max(wideWindowMinPaddingPx, paddingPx);
    }

    /**
     * Computes the horizontal padding for a view, applying wide-display constraints if necessary.
     *
     * <p>If the display is narrow, it returns the {@code defaultPaddingPixels}. If the display is
     * wide, it calculates "centering padding" to ensure the content stays within a readable maximum
     * width, ensuring the result is at least {@code minWidePaddingPixels}.
     *
     * @param view The {@link View} to calculate padding for.
     * @param uiConfig The {@link UiConfig} to check the current display style.
     * @param defaultPaddingPixels The standard padding for narrow/phone layouts.
     * @param minWidePaddingPixels The baseline padding for wide/tablet layouts.
     * @return The horizontal padding in pixels.
     * @see ViewResizerUtil#computePaddingForWideDisplay
     */
    public static int computePadding(
            View view, UiConfig uiConfig, int defaultPaddingPixels, int minWidePaddingPixels) {
        if (!uiConfig.getCurrentDisplayStyle().isWide()) return defaultPaddingPixels;
        return ViewResizerUtil.computePaddingForWideDisplay(
                uiConfig.getContext(), view, minWidePaddingPixels);
    }
}
