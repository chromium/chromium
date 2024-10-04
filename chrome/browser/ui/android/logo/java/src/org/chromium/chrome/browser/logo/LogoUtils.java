// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.res.Resources;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;

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

    private static final String LOGO_POLISH_LARGE_SIZE_PARAM = "polish_logo_size_large";
    public static final BooleanCachedFieldTrialParameter LOGO_POLISH_LARGE_SIZE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LOGO_POLISH, LOGO_POLISH_LARGE_SIZE_PARAM, false);

    private static final String LOGO_POLISH_MEDIUM_SIZE_PARAM = "polish_logo_size_medium";
    public static final BooleanCachedFieldTrialParameter LOGO_POLISH_MEDIUM_SIZE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LOGO_POLISH, LOGO_POLISH_MEDIUM_SIZE_PARAM, true);

    /** Returns whether logo polish flag is enabled in the given context. */
    public static boolean isLogoPolishEnabled() {
        return ChromeFeatureList.sLogoPolish.isEnabled();
    }

    /**
     * Returns whether logo is Google doodle and logo polish is enabled in the given context.
     *
     * @param isLogoDoodle True if the current logo is Google doodle.
     */
    public static boolean isLogoPolishEnabledWithGoogleDoodle(boolean isLogoDoodle) {
        return isLogoDoodle && isLogoPolishEnabled();
    }

    /**
     * Returns the logo size to use when logo polish is enabled. When logo polish is disabled, the
     * return value should be invalid.
     */
    public static @LogoSizeForLogoPolish int getLogoSizeForLogoPolish() {
        if (LOGO_POLISH_LARGE_SIZE.getValue()) {
            return LogoSizeForLogoPolish.LARGE;
        }

        if (LOGO_POLISH_MEDIUM_SIZE.getValue()) {
            return LogoSizeForLogoPolish.MEDIUM;
        }

        return LogoSizeForLogoPolish.SMALL;
    }

    /** Returns the top margin of the LogoView if Logo Polish is enabled. */
    public static int getTopMarginForLogoPolish(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_margin_top_logo_polish);
    }

    /** Returns the height of the LogoView if Logo Polish is enabled with large height. */
    @VisibleForTesting
    public static int getLogoHeightForLogoPolishWithLargeSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_large);
    }

    /** Returns the height of the LogoView if Logo Polish is enabled with medium height. */
    @VisibleForTesting
    public static int getLogoHeightForLogoPolishWithMediumSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_medium);
    }

    /** Returns the height of the LogoView if Logo Polish is enabled. */
    @VisibleForTesting
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

        if (layoutParams.height == logoHeight) {
            return;
        }

        layoutParams.height = logoHeight;
        layoutParams.topMargin = logoTopMargin;
        logoView.setLayoutParams(layoutParams);
    }
}
