// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the {@link CachedTintedBitmap}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CachedTintedBitmapUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testGetDefaultGoogleLogoAndGoogleIsDse() {
        @DrawableRes int drawableId = R.drawable.google_logo;
        @ColorRes int colorId = R.color.google_logo_tint_color;
        Assert.assertEquals(Color.TRANSPARENT, mContext.getColor(colorId));
        // Build verifyLogo for later comparison.
        Bitmap verifyLogo = BitmapFactory.decodeResource(mContext.getResources(), drawableId);

        CachedTintedBitmap cachedTintedBitmap = new CachedTintedBitmap(drawableId, colorId);
        Assert.assertTrue(verifyLogo.sameAs(cachedTintedBitmap.getBitmap(mContext)));
        Assert.assertTrue(
                verifyLogo.sameAs(cachedTintedBitmap.getPreviousBitmapForTesting().get()));
        Assert.assertEquals(Color.TRANSPARENT, cachedTintedBitmap.getPreviousTintForTesting());

        // When the bitmap is already up-to-date, nothing will change.
        Assert.assertTrue(verifyLogo.sameAs(cachedTintedBitmap.getBitmap(mContext)));
        Assert.assertTrue(
                verifyLogo.sameAs(cachedTintedBitmap.getPreviousBitmapForTesting().get()));
        Assert.assertEquals(Color.TRANSPARENT, cachedTintedBitmap.getPreviousTintForTesting());
    }
}
