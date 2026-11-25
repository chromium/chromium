// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TopPageColorObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TopPageColorObserverUnitTest {
    @Test
    public void testGetDominantColor_SolidColor() {
        Bitmap bitmap = Bitmap.createBitmap(10, 6, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.BLUE);

        assertEquals(Integer.valueOf(Color.BLUE), TopPageColorObserver.getDominantColor(bitmap));
    }

    @Test
    public void testGetDominantColor_MixedColorsPicksMostFrequent() {
        Bitmap bitmap = Bitmap.createBitmap(10, 6, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.BLUE);
        bitmap.setPixel(0, 0, Color.RED);
        bitmap.setPixel(9, 0, Color.RED);

        assertEquals(Integer.valueOf(Color.BLUE), TopPageColorObserver.getDominantColor(bitmap));
    }

    @Test
    public void testGetDominantColor_AllTransparentReturnsNull() {
        Bitmap bitmap = Bitmap.createBitmap(10, 6, Bitmap.Config.ARGB_8888);
        bitmap.setPixel(0, 0, Color.argb(128, 255, 0, 0));

        assertNull(TopPageColorObserver.getDominantColor(bitmap));
    }

    @Test
    public void testGetDominantColor_BelowHalfThresholdReturnsNull() {
        Bitmap bitmap = Bitmap.createBitmap(10, 1, Bitmap.Config.ARGB_8888);
        // 4 blue pixels (40%), 4 red pixels (40%), 2 semi-transparent green pixels (ignored).
        for (int x = 0; x < 4; x++) {
            bitmap.setPixel(x, 0, Color.BLUE);
        }
        for (int x = 4; x < 8; x++) {
            bitmap.setPixel(x, 0, Color.RED);
        }
        for (int x = 8; x < 10; x++) {
            bitmap.setPixel(x, 0, Color.argb(128, 0, 255, 0));
        }

        assertNull(TopPageColorObserver.getDominantColor(bitmap));
    }

    @Test
    public void testGetDominantColor_ExactlyHalfThresholdReturnsColor() {
        Bitmap bitmap = Bitmap.createBitmap(10, 1, Bitmap.Config.ARGB_8888);
        // 5 blue pixels (exactly 50%).
        for (int x = 0; x < 5; x++) {
            bitmap.setPixel(x, 0, Color.BLUE);
        }
        // 5 opaque colors that are far apart from blue and from each other, so none of them forms
        // a cluster of more than one pixel.
        bitmap.setPixel(5, 0, Color.RED);
        bitmap.setPixel(6, 0, Color.GREEN);
        bitmap.setPixel(7, 0, Color.YELLOW);
        bitmap.setPixel(8, 0, Color.BLACK);
        bitmap.setPixel(9, 0, Color.WHITE);

        assertEquals(Integer.valueOf(Color.BLUE), TopPageColorObserver.getDominantColor(bitmap));
    }

    @Test
    public void testGetDominantColor_DensestBallClustersNearbyColors() {
        Bitmap bitmap = Bitmap.createBitmap(10, 1, Bitmap.Config.ARGB_8888);
        // 4 exact red pixels (40%).
        for (int x = 0; x < 4; x++) {
            bitmap.setPixel(x, 0, Color.RED);
        }
        // 6 slightly varying blue shades (60% total), where no single shade has >= 50% on its own,
        // and each shade has fewer exact matches than RED (3, 2, 1 vs 4), but all 6 are within
        // MAX_COLOR_DISTANCE_SQUARED of centerBlue.
        int nearBlue1 = Color.rgb(0, 0, 248);
        int centerBlue = Color.rgb(2, 2, 250);
        int nearBlue2 = Color.rgb(4, 4, 252);
        bitmap.setPixel(4, 0, centerBlue);
        bitmap.setPixel(5, 0, centerBlue);
        bitmap.setPixel(6, 0, centerBlue);
        bitmap.setPixel(7, 0, nearBlue1);
        bitmap.setPixel(8, 0, nearBlue1);
        bitmap.setPixel(9, 0, nearBlue2);

        assertEquals(Integer.valueOf(centerBlue), TopPageColorObserver.getDominantColor(bitmap));
    }

    @Test
    public void testGetDominantColor_DistanceBoundaryDecidesClustering() {
        Bitmap bitmap = Bitmap.createBitmap(10, 1, Bitmap.Config.ARGB_8888);
        // 4 white, 4 black, 2 near-black pixels. Whether the near-black pixels join the black
        // cluster decides whether black reaches the 50% threshold.
        for (int x = 0; x < 4; x++) {
            bitmap.setPixel(x, 0, Color.WHITE);
        }
        for (int x = 4; x < 8; x++) {
            bitmap.setPixel(x, 0, Color.BLACK);
        }

        // Squared distance to black is just above the threshold, so the
        // largest cluster has only 4 pixels (40%).
        bitmap.setPixel(8, 0, Color.rgb(10, 10, 11));
        bitmap.setPixel(9, 0, Color.rgb(10, 10, 11));
        assertNull(TopPageColorObserver.getDominantColor(bitmap));

        // Squared distance to black is exactly the threshold, so the near-black pixels join the
        // black cluster (6 pixels, 60%). Black is returned rather than
        // the near-black shade because it has more exact matches.
        bitmap.setPixel(8, 0, Color.rgb(10, 10, 10));
        bitmap.setPixel(9, 0, Color.rgb(10, 10, 10));
        assertEquals(Integer.valueOf(Color.BLACK), TopPageColorObserver.getDominantColor(bitmap));
    }

    @Test
    public void testGetDominantColor_OddPixelCountUsesAtLeastHalf() {
        Bitmap bitmap = Bitmap.createBitmap(5, 1, Bitmap.Config.ARGB_8888);
        // 2 of 5 pixels (40%) are blue; the rest are far apart from blue and from each other.
        bitmap.setPixel(0, 0, Color.BLUE);
        bitmap.setPixel(1, 0, Color.BLUE);
        bitmap.setPixel(2, 0, Color.RED);
        bitmap.setPixel(3, 0, Color.GREEN);
        bitmap.setPixel(4, 0, Color.YELLOW);
        assertNull(TopPageColorObserver.getDominantColor(bitmap));

        // 3 of 5 pixels (60%) are blue, which is the smallest count that covers at least half of
        // an odd number of pixels.
        bitmap.setPixel(2, 0, Color.BLUE);
        assertEquals(Integer.valueOf(Color.BLUE), TopPageColorObserver.getDominantColor(bitmap));
    }
}
