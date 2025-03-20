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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoUtils.DoodleSize;

/** Unit tests for the {@link LogoViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Resources mResources;
    @Mock private LogoView mLogoView;

    @Before
    public void setUp() {
        mResources = Robolectric.buildActivity(Activity.class).setup().get().getResources();
    }

    @Test
    @SmallTest
    public void testSetLogoViewLayoutParamsForDoodle() {
        MarginLayoutParams layoutParams = new MarginLayoutParams(0, 0);
        when(mLogoView.getLayoutParams()).thenReturn(layoutParams);

        int doodleHeight = mResources.getDimensionPixelSize(R.dimen.doodle_height);
        int doodleHeightForTabletSplitScreen =
                mResources.getDimensionPixelSize(R.dimen.doodle_height_tablet_split_screen);
        int doodleTopMargin = mResources.getDimensionPixelSize(R.dimen.doodle_margin_top);

        // Verifies the layout params for doodle.
        LogoUtils.setLogoViewLayoutParamsForDoodle(
                mLogoView, mResources, /* doodleSize= */ DoodleSize.REGULAR);
        testSetLogoViewLayoutParamsForDoodleImpl(doodleHeight, doodleTopMargin, layoutParams);

        LogoUtils.setLogoViewLayoutParamsForDoodle(
                mLogoView, mResources, /* doodleSize= */ DoodleSize.TABLET_SPLIT_SCREEN);
        testSetLogoViewLayoutParamsForDoodleImpl(
                doodleHeightForTabletSplitScreen, doodleTopMargin, layoutParams);
    }

    private void testSetLogoViewLayoutParamsForDoodleImpl(
            int logoHeight, int logoTopMargin, MarginLayoutParams layoutParams) {
        Assert.assertEquals(logoHeight, layoutParams.height);
        Assert.assertEquals(logoTopMargin, layoutParams.topMargin);
    }
}
