// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.res.Resources;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility classes for {@link LogoView} */
public class LogoUtils {
    /** Used to specify the logo size when Logo Polish is enabled. */
    @IntDef({
        LogoSizeForLogoPolish.SMALL,
        LogoSizeForLogoPolish.MEDIUM,
        LogoSizeForLogoPolish.LARGE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LogoSizeForLogoPolish {
        int SMALL = 0;
        int MEDIUM = 1;
        int LARGE = 2;
    }

    /** Returns the top margin of the LogoView if Surface Polish is enabled. */
    public static int getTopMarginPolished(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_top_polished);
    }

    /** Returns the bottom margin of the LogoView if Surface Polish is enabled. */
    public static int getBottomMarginPolished(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_bottom_polished);
    }

    @VisibleForTesting
    /** Returns the height of the LogoView if Surface Polish is enabled. */
    public static int getLogoHeightPolished(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_polished);
    }

    /** Returns the top margin of the LogoView if Logo Polish is enabled. */
    public static int getTopMarginForLogoPolish(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_top_logo_polish);
    }

    /** Returns the bottom margin of the LogoView if Logo Polish is enabled. */
    public static int getBottomMarginForLogoPolish(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_bottom_logo_polish);
    }

    @VisibleForTesting
    /** Returns the height of the LogoView if Logo Polish is enabled with large height. */
    public static int getLogoHeightForLogoPolishWithLargeSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_large);
    }

    @VisibleForTesting
    /** Returns the height of the LogoView if Logo Polish is enabled with medium height. */
    public static int getLogoHeightForLogoPolishWithMediumSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_medium);
    }

    @VisibleForTesting
    /** Returns the height of the LogoView if Logo Polish is enabled. */
    public static int getLogoHeightForLogoPolishWithSmallSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_small);
    }

    /**
     * Returns the sum of the height, the top margin and the bottom margin of the LogoView if
     * Surface Polish is enabled.
     */
    public static int getLogoTotalHeightPolished(Resources resources) {
        return getLogoHeightPolished(resources)
                + getTopMarginPolished(resources)
                + getBottomMarginPolished(resources);
    }

    /**
     * Returns the sum of the height, the top margin and the bottom margin of the LogoView if Logo
     * Polish is enabled.
     *
     * @param resources The current Android's resources.
     * @param logoSize The type of the logo size to be used for the logo.
     */
    public static int getLogoTotalHeightForLogoPolish(
            Resources resources, final @LogoSizeForLogoPolish int logoSize) {
        switch (logoSize) {
            case LogoSizeForLogoPolish.LARGE:
                return getLogoHeightForLogoPolishWithLargeSize(resources)
                        + getBottomMarginForLogoPolish(resources)
                        + getTopMarginForLogoPolish(resources);
            case LogoSizeForLogoPolish.MEDIUM:
                return getLogoHeightForLogoPolishWithMediumSize(resources)
                        + getBottomMarginForLogoPolish(resources)
                        + getTopMarginForLogoPolish(resources);
            case LogoSizeForLogoPolish.SMALL:
                return getLogoHeightForLogoPolishWithSmallSize(resources)
                        + getBottomMarginForLogoPolish(resources)
                        + getTopMarginForLogoPolish(resources);
            default:
                assert false;
                return getLogoTotalHeightPolished(resources);
        }
    }

    /**
     * Sets the layout params for the LogoView when Surface Polished or Logo Polished is enabled.
     */
    public static void setLogoViewLayoutParams(
            LogoView logoView,
            Resources resources,
            boolean isTablet,
            boolean isLogoPolishEnabled,
            final @LogoSizeForLogoPolish int logoSizeForLogoPolish) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        setLogoViewLayoutParams(
                layoutParams, resources, isTablet, isLogoPolishEnabled, logoSizeForLogoPolish);
        if (layoutParams != null) {
            logoView.setLayoutParams(layoutParams);
        }
    }

    @VisibleForTesting
    public static void setLogoViewLayoutParams(
            MarginLayoutParams layoutParams,
            Resources resources,
            boolean isTablet,
            boolean isLogoPolishEnabled,
            final @LogoSizeForLogoPolish int logoSizeForLogoPolish) {
        if (layoutParams == null) return;

        if (isLogoPolishEnabled) {
            switch (logoSizeForLogoPolish) {
                case LogoSizeForLogoPolish.LARGE:
                    layoutParams.height = getLogoHeightForLogoPolishWithLargeSize(resources);
                    layoutParams.topMargin = getTopMarginForLogoPolish(resources);
                    layoutParams.bottomMargin = getBottomMarginForLogoPolish(resources);
                    break;
                case LogoSizeForLogoPolish.MEDIUM:
                    layoutParams.height = getLogoHeightForLogoPolishWithMediumSize(resources);
                    layoutParams.topMargin = getTopMarginForLogoPolish(resources);
                    layoutParams.bottomMargin = getBottomMarginForLogoPolish(resources);
                    break;
                case LogoSizeForLogoPolish.SMALL:
                    layoutParams.height = getLogoHeightForLogoPolishWithSmallSize(resources);
                    layoutParams.topMargin = getTopMarginForLogoPolish(resources);
                    layoutParams.bottomMargin = getBottomMarginForLogoPolish(resources);
                    break;
                default:
                    assert false;
            }
        } else {
            layoutParams.height = getLogoHeightPolished(resources);
            layoutParams.topMargin = getTopMarginPolished(resources);
            layoutParams.bottomMargin = getBottomMarginPolished(resources);
        }
    }
}
