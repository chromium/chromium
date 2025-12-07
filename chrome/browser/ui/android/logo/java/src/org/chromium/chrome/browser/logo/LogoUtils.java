// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.res.Resources;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility classes for {@link LogoView} */
@NullMarked
public class LogoUtils {
    /**
     * Used to specify the logo size when the current logo is a google doodle. REGULAR size is used
     * in most cases while the TABLET_SPLIT_SCREEN size is used only in split screens on tablets.
     */
    @IntDef({DoodleSize.TABLET_SPLIT_SCREEN, DoodleSize.REGULAR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DoodleSize {
        int TABLET_SPLIT_SCREEN = 0;
        int REGULAR = 1;
    }

    /** Returns the top margin of the LogoView when the current logo is a google doodle. */
    public static int getTopMarginForDoodle(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.doodle_margin_top);
    }

    /** Returns the height of the LogoView when the current logo is a google doodle. */
    @VisibleForTesting
    public static int getDoodleHeight(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.doodle_height);
    }

    /**
     * Returns the height of the LogoView when the current logo is a google doodle and in split
     * screens on tablets.
     */
    @VisibleForTesting
    public static int getDoodleHeightInTabletSplitScreen(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.doodle_height_tablet_split_screen);
    }

    /** Sets the layout params for the LogoView when the current logo is a google doodle. */
    @VisibleForTesting
    public static void setLogoViewLayoutParamsForDoodle(
            LogoView logoView, Resources resources, final @DoodleSize int doodleSize) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) logoView.getLayoutParams();
        if (layoutParams == null) return;

        int[] logoViewLayoutParams =
                getLogoViewLayoutParams(resources, /* isLogoDoodle= */ true, doodleSize);
        setLogoViewLayoutParamsForDoodle(
                logoView, logoViewLayoutParams[0], logoViewLayoutParams[1]);
    }

    public static int[] getLogoViewLayoutParams(
            Resources resources, boolean isLogoDoodle, final @DoodleSize int doodleSize) {
        if (isLogoDoodle) {
            switch (doodleSize) {
                case DoodleSize.REGULAR:
                    return new int[] {
                        getDoodleHeight(resources), getTopMarginForDoodle(resources),
                    };
                case DoodleSize.TABLET_SPLIT_SCREEN:
                    return new int[] {
                        getDoodleHeightInTabletSplitScreen(resources),
                        getTopMarginForDoodle(resources),
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

    public static void setLogoViewLayoutParamsForDoodle(
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
