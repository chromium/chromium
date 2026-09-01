// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Robolectric unit tests for {@link BitmapUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BitmapUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapByDimensions_smallerThanMax_returnsOriginal() {
        Bitmap bitmap = Bitmap.createBitmap(100, 50, Bitmap.Config.ARGB_8888);
        Bitmap result = BitmapUtils.resizeBitmapByDimensions(bitmap, 200, 100);
        Assert.assertSame(bitmap, result);
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapByDimensions_exactDimensions_returnsOriginal() {
        Bitmap bitmap = Bitmap.createBitmap(200, 100, Bitmap.Config.ARGB_8888);
        Bitmap result = BitmapUtils.resizeBitmapByDimensions(bitmap, 200, 100);
        Assert.assertSame(bitmap, result);
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapByDimensions_widerThanMax_scalesPreservingAspectRatio() {
        Bitmap bitmap = Bitmap.createBitmap(2000, 1000, Bitmap.Config.ARGB_8888);
        Bitmap result = BitmapUtils.resizeBitmapByDimensions(bitmap, 500, 500);
        Assert.assertEquals(500, result.getWidth());
        Assert.assertEquals(250, result.getHeight());
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapByDimensions_tallerThanMax_scalesPreservingAspectRatio() {
        Bitmap bitmap = Bitmap.createBitmap(1000, 2000, Bitmap.Config.ARGB_8888);
        Bitmap result = BitmapUtils.resizeBitmapByDimensions(bitmap, 500, 500);
        Assert.assertEquals(250, result.getWidth());
        Assert.assertEquals(500, result.getHeight());
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapByDimensions_bothDimensionsExceed_scalesByDominantRatio() {
        Bitmap bitmap = Bitmap.createBitmap(3000, 1000, Bitmap.Config.ARGB_8888);
        Bitmap result = BitmapUtils.resizeBitmapByDimensions(bitmap, 600, 400);
        Assert.assertEquals(600, result.getWidth());
        Assert.assertEquals(200, result.getHeight());
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapByMemory_underLimit_returnsOriginal() {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        int sizeInKb = bitmap.getAllocationByteCount() / 1000;
        Bitmap result = BitmapUtils.resizeBitmapByMemory(bitmap, sizeInKb + 100);
        Assert.assertSame(bitmap, result);
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapByMemory_exceedsLimit_scalesDownToFit() {
        Bitmap bitmap = Bitmap.createBitmap(2000, 2000, Bitmap.Config.ARGB_8888);
        int initialSizeInKb = bitmap.getAllocationByteCount() / 1000;
        int targetSizeInKb = initialSizeInKb / 4;

        Bitmap result = BitmapUtils.resizeBitmapByMemory(bitmap, targetSizeInKb);
        int resultingSizeInKb = result.getAllocationByteCount() / 1000;
        Assert.assertTrue(resultingSizeInKb <= targetSizeInKb);
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapIfNeeded_withinBoundsAndMemory_returnsOriginal() {
        float density = mContext.getResources().getDisplayMetrics().density;
        int widthDp = 100;
        int heightDp = 50;
        int widthPx = Math.round(widthDp * density);
        int heightPx = Math.round(heightDp * density);

        Bitmap bitmap = Bitmap.createBitmap(widthPx, heightPx, Bitmap.Config.ARGB_8888);
        int maxKBytes = (bitmap.getAllocationByteCount() / 1000) + 100;

        Bitmap result =
                BitmapUtils.resizeBitmapIfNeeded(mContext, bitmap, widthDp, heightDp, maxKBytes);
        Assert.assertSame(bitmap, result);
    }

    @Test
    @Feature({"Browser", "Notifications"})
    public void testResizeBitmapIfNeeded_convertsDpUsingDisplayDensityAndScales() {
        Bitmap bitmap = Bitmap.createBitmap(2000, 1000, Bitmap.Config.ARGB_8888);

        int maxWidthDp = 416;
        int maxHeightDp = 284;
        int maxKBytes = 4500;

        Bitmap result =
                BitmapUtils.resizeBitmapIfNeeded(
                        mContext, bitmap, maxWidthDp, maxHeightDp, maxKBytes);
        Assert.assertNotNull(result);

        float density = mContext.getResources().getDisplayMetrics().density;
        int expectedWidth = Math.round(maxWidthDp * density);
        int expectedHeight = Math.round((float) expectedWidth * 1000 / 2000);

        Assert.assertEquals(expectedWidth, result.getWidth());
        Assert.assertEquals(expectedHeight, result.getHeight());
        Assert.assertTrue(result.getAllocationByteCount() / 1000 <= maxKBytes);
    }
}
