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

        LogoUtils.setLogoViewLayoutParams(layoutParams, mResources, false, true);
        Assert.assertEquals(logoHeightShort, layoutParams.height);
        Assert.assertEquals(logoTopMarginSmall, layoutParams.topMargin);
        Assert.assertEquals(logoBottomMarginSmall, layoutParams.bottomMargin);

        LogoUtils.setLogoViewLayoutParams(layoutParams, mResources, false, false);
        Assert.assertEquals(logoHeight, layoutParams.height);
        Assert.assertEquals(logoTopMargin, layoutParams.topMargin);
        Assert.assertEquals(logoBottomMargin, layoutParams.bottomMargin);

        // Verifies that less brand space isn't used on tablets.
        LogoUtils.setLogoViewLayoutParams(layoutParams, mResources, true, false);
        Assert.assertEquals(logoHeight, layoutParams.height);
        Assert.assertEquals(logoTopMargin, layoutParams.topMargin);
        Assert.assertEquals(logoBottomMargin, layoutParams.bottomMargin);

        LogoUtils.setLogoViewLayoutParams(layoutParams, mResources, true, true);
        Assert.assertEquals(logoHeight, layoutParams.height);
        Assert.assertEquals(logoTopMargin, layoutParams.topMargin);
        Assert.assertEquals(logoBottomMargin, layoutParams.bottomMargin);
    }
}
