// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.displaystyle;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.display.DisplayUtil;

/** Util class for @{@link org.chromium.components.browser_ui.widget.displaystyle.ViewResizer}. */
@NullMarked
public class ViewResizerUtil {

    /**
     * When layout has a wide display style, compute padding to constrain to {@link
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}.
     */
    public static int computePaddingForWideDisplay(
            Context context, @Nullable View view, int minWidePaddingPixels) {
        Resources resources = context.getResources();
        float dpToPx = resources.getDisplayMetrics().density;

        int screenWidthDp = 0;
        // Some automotive devices with wide displays show side UI components, which should not be
        // included in the padding calculations.
        if (DisplayUtil.isUiScaled() && view != null) {
            screenWidthDp = (int) (view.getMeasuredWidth() / dpToPx);
        }
        if (screenWidthDp == 0) {
            screenWidthDp = resources.getConfiguration().screenWidthDp;
        }

        int padding =
                (int) (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f) * dpToPx);
        return Math.max(minWidePaddingPixels, padding);
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
