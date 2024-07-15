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

    /** Returns the top margin of the LogoView if Logo Polish is enabled. */
    public static int getTopMarginForLogoPolish(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_top_logo_polish);
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

    /** Returns the sum of the height, the top margin and the bottom margin of the LogoView. */
    public static int getLogoTotalHeight(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.ntp_logo_height)
                + resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top)
                + resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);
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
        int bottomMargin = resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);
        switch (logoSize) {
            case LogoSizeForLogoPolish.LARGE:
                return getLogoHeightForLogoPolishWithLargeSize(resources)
                        + getTopMarginForLogoPolish(resources)
                        + bottomMargin;
            case LogoSizeForLogoPolish.MEDIUM:
                return getLogoHeightForLogoPolishWithMediumSize(resources)
                        + getTopMarginForLogoPolish(resources)
                        + bottomMargin;
            case LogoSizeForLogoPolish.SMALL:
                return getLogoHeightForLogoPolishWithSmallSize(resources)
                        + getTopMarginForLogoPolish(resources)
                        + bottomMargin;
            default:
                assert false;
                return getLogoTotalHeight(resources);
        }
    }

    /** Sets the layout params for the LogoView when Logo Polished is enabled. */
    @VisibleForTesting
    public static void setLogoViewLayoutParams(
            LogoView logoView,
            Resources resources,
            boolean isLogoPolishEnabled,
            final @LogoSizeForLogoPolish int logoSizeForLogoPolish) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        if (layoutParams == null) return;

        int[] logoViewLayoutParams =
                getLogoViewLayoutParams(resources, isLogoPolishEnabled, logoSizeForLogoPolish);
        setLogoViewLayoutParams(logoView, logoViewLayoutParams[0], logoViewLayoutParams[1]);
    }

    public static int[] getLogoViewLayoutParams(
            Resources resources,
            boolean isLogoPolishEnabled,
            final @LogoSizeForLogoPolish int logoSizeForLogoPolish) {
        if (isLogoPolishEnabled) {
            switch (logoSizeForLogoPolish) {
                case LogoSizeForLogoPolish.LARGE:
                    return new int[] {
                        getLogoHeightForLogoPolishWithLargeSize(resources),
                        getTopMarginForLogoPolish(resources),
                    };
                case LogoSizeForLogoPolish.MEDIUM:
                    return new int[] {
                        getLogoHeightForLogoPolishWithMediumSize(resources),
                        getTopMarginForLogoPolish(resources),
                    };
                case LogoSizeForLogoPolish.SMALL:
                    return new int[] {
                        getLogoHeightForLogoPolishWithSmallSize(resources),
                        getTopMarginForLogoPolish(resources),
                    };
                default:
                    assert false;
            }
        } else {
            return new int[] {
                resources.getDimensionPixelSize(R.dimen.ntp_logo_height),
                resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top),
            };
        }
        return new int[] {0, 0};
    }

    public static void setLogoViewLayoutParams(
            LogoView logoView, int logoHeight, int logoTopMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        layoutParams.height = logoHeight;
        layoutParams.topMargin = logoTopMargin;
        logoView.setLayoutParams(layoutParams);
    }
}
