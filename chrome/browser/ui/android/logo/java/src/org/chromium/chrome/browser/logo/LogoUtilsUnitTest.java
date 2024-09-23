// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoUtils.LogoSizeForLogoPolish;

/** Unit tests for the {@link LogoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoUtilsUnitTest {
    @Mock private Resources mResources;
    @Mock private LogoView mLogoView;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mResources = Robolectric.buildActivity(Activity.class).setup().get().getResources();
    }

    @Test
    @SmallTest
    public void testSetLogoViewLayoutParams() {
        MarginLayoutParams layoutParams = new MarginLayoutParams(0, 0);
        when(mLogoView.getLayoutParams()).thenReturn(layoutParams);

        int logoHeight = mResources.getDimensionPixelSize(R.dimen.ntp_logo_height);
        int logoTopMargin = mResources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);

        int logoHeightLargeForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_large);
        int logoHeightMediumForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_medium);
        int logoHeightSmallForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_height_logo_polish_small);
        int logoTopMarginForLogoPolish =
                mResources.getDimensionPixelSize(R.dimen.logo_margin_top_logo_polish);

        LogoUtils.setLogoViewLayoutParams(
                mLogoView,
                mResources,
                /* isLogoPolishEnabled= */ false,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(logoHeight, logoTopMargin, layoutParams);

        // Verifies the layout params for Logo Polish.
        LogoUtils.setLogoViewLayoutParams(
                mLogoView,
                mResources,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.LARGE);
        testSetLogoViewLayoutParamsImpl(
                logoHeightLargeForLogoPolish, logoTopMarginForLogoPolish, layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                mLogoView,
                mResources,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.MEDIUM);
        testSetLogoViewLayoutParamsImpl(
                logoHeightMediumForLogoPolish, logoTopMarginForLogoPolish, layoutParams);

        LogoUtils.setLogoViewLayoutParams(
                mLogoView,
                mResources,
                /* isLogoPolishEnabled= */ true,
                /* logoSizeForLogoPolish= */ LogoSizeForLogoPolish.SMALL);
        testSetLogoViewLayoutParamsImpl(
                logoHeightSmallForLogoPolish, logoTopMarginForLogoPolish, layoutParams);
    }

    private void testSetLogoViewLayoutParamsImpl(
            int logoHeight, int logoTopMargin, MarginLayoutParams layoutParams) {
        Assert.assertEquals(logoHeight, layoutParams.height);
        Assert.assertEquals(logoTopMargin, layoutParams.topMargin);
    }
}
