// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.res.Resources;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.VisibleForTesting;

/** Utility classes for {@link LogoView} */
public class LogoUtils {
    /**
     * Returns the top margin of the LogoView if Surface Polish is enabled and with less brand
     * space.
     */
    public static int getTopMarginPolishedSmall(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_top_polished_small);
    }

    /**
     * Returns the top margin of the LogoView if Surface Polish is enabled.
     */
    public static int getTopMarginPolished(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_top_polished);
    }

    /**
     * Returns the bottom margin of the LogoView if Surface Polish is enabled and with less brand
     * space.
     */
    public static int getBottomMarginPolishedSmall(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_bottom_polished_small);
    }

    /**
     * Returns the bottom margin of the LogoView if Surface Polish.
     */
    public static int getBottomMarginPolished(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_bottom_polished);
    }

    /**
     * Returns the height of the LogoView if Surface Polish and with less brand space.
     */
    public static int getLogoHeightPolished(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_polished);
    }

    /**
     * Returns the height of the LogoView if Surface Polish.
     */
    public static int getLogoHeightPolishedShort(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_short);
    }

    /**
     * Sets the layout params for the LogoView when Surface Polished is enabled.
     */
    public static void setLogoViewLayoutParams(
            LogoView logoView, Resources resources, boolean isTablet, boolean useLessBrandSpace) {
        setLogoViewLayoutParams((MarginLayoutParams) logoView.getLayoutParams(), resources,
                isTablet, useLessBrandSpace);
    }

    @VisibleForTesting
    public static void setLogoViewLayoutParams(MarginLayoutParams layoutParams, Resources resources,
            boolean isTablet, boolean useLessBrandSpace) {
        if (layoutParams == null) return;

        if (useLessBrandSpace && !isTablet) {
            layoutParams.height = getLogoHeightPolishedShort(resources);
            layoutParams.topMargin = getTopMarginPolishedSmall(resources);
            layoutParams.bottomMargin = getBottomMarginPolishedSmall(resources);
        } else {
            layoutParams.height = getLogoHeightPolished(resources);
            layoutParams.topMargin = getTopMarginPolished(resources);
            layoutParams.bottomMargin = getBottomMarginPolished(resources);
        }
    }
}
