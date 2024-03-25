// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.app.Activity;
import android.content.res.Resources;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoUtils.LogoSizeForLogoPolish;

/** Unit tests for the {@link LogoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoUtilsUnitTest {
    @Mock private Resources mResources;

    @Before
    public void setUp() {
        mResources = Robolectric.buildActivity(Activity.class).setup().get().getResources();
    }

    @Test
    @SmallTest
    public void testSetLogoViewLayoutParams() {
        MarginLayoutParams layoutParams = new MarginLayoutParams(0, 0);
        int logoHeight = mResources.getDimensionPixelSize(R.dimen.logo_height_polished);
        int logoHeightShort = mResources.getDimensionPixelSize(R.dimen.logo_height_short);
        int logoTopMarginSmall =
                mResources.getDimensionPixelSize(R.dimen.logo_margin_top_polished_small);
        int logoTopMargin = mResources.getDimensionPixelSize(R.dimen.logo_margin_top_polished);
        int logoBottomMarginSmall =
                mResources.getDimensionPixelSize(R.dimen.logo_margin_bottom_polished_small);
        int logoBottomMargin =
                mResources.getDimensionPixelSize(R.dimen.logo_margin_bottom_polished);

        int logoHeightLargeForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_large);
        int logoHeightMediumForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_medium);
        int logoHeightSmallForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_small);
        int logoTopMarginForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_margin_top_logo_polish);
        int logoBottomMarginForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_margin_bottom_logo_polish);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ false,
                /* useLessBrandSpace= */ true,
                /* isLogoPolishEnabled= */ false,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(
                logoHeightShort, logoTopMarginSmall, logoBottomMarginSmall, layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ false,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ false,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(logoHeight, logoTopMargin, logoBottomMargin, layoutParams);

        // Verifies that less brand space isn't used on tablets.
        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ true,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ false,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(logoHeight, logoTopMargin, logoBottomMargin, layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ true,
                /* useLessBrandSpace= */ true,
                /* isLogoPolishEnabled= */ false,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(logoHeight, logoTopMargin, logoBottomMargin, layoutParams);

        // Verifies the layout params for Logo Polish.
        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ false,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(
                logoHeightLargeForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ false,
                /* useLessBrandSpace= */ true,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(
                logoHeightLargeForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ true,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(
                logoHeightLargeForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ true,
                /* useLessBrandSpace= */ true,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(
                logoHeightLargeForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ false,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.MEDIUM);
        testSetLogoViewLayoutParamsImpl(
                logoHeightMediumForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ true,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.MEDIUM);
        testSetLogoViewLayoutParamsImpl(
                logoHeightMediumForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ false,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.SMALL);
        testSetLogoViewLayoutParamsImpl(
                logoHeightSmallForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                layoutParams,
                mResources,
                /* isTablet= */ true,
                /* useLessBrandSpace= */ false,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.SMALL);
        testSetLogoViewLayoutParamsImpl(
                logoHeightSmallForLogoPolish,
                logoTopMarginForLogoPolish,
                logoBottomMarginForLogoPolish,
                layoutParams);
    }

    private void testSetLogoViewLayoutParamsImpl(
            int logoHeight,
            int logoTopMargin,
            int logoBottomMargin,
            MarginLayoutParams layoutParams) {
        Assert.assertEquals(logoHeight, layoutParams.height);
        Assert.assertEquals(logoTopMargin, layoutParams.topMargin);
        Assert.assertEquals(logoBottomMargin, layoutParams.bottomMargin);
    }
}
