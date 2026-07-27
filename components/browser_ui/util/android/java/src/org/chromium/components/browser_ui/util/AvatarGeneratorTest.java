// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import static org.junit.Assert.assertTrue;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.Arrays;

/**
 * Unit tests for {@link AvatarGenerator}. It cannot be run as roboelectric test since it uses
 * Canvas.drawBitmap() which isn't supported for in Roboelectric tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AvatarGeneratorTest {
    private static final int IMAGE_SIZE_PX = 80;
    private final Resources mResources = ContextUtils.getApplicationContext().getResources();
    private final float mMargin = 1 * mResources.getDisplayMetrics().density;
    private final int mLeftHalfEnd = (int) (IMAGE_SIZE_PX / 2 - mMargin);
    private final int mTopHalfEnd = (int) (IMAGE_SIZE_PX / 2 - mMargin);
    private final int mRightHalfStart = (int) (IMAGE_SIZE_PX / 2 + mMargin);
    private final int mBottomHalfStart = (int) (IMAGE_SIZE_PX / 2 + mMargin);

    /**
     * Creates a Bitmap that have the center as shown below
     *
     * <pre>
     * +-------+------------------+--------+
     * |       |------------------|        |
     * |       |------------------|        |
     * |       |------------------|        |
     * |       |------------------|        |
     * |       |------------------|        |
     * +-------+------------------+--------+
     * </pre>
     *
     * @param leftColor The color used for the left slice.
     * @param middleColor The color used for the middle slice.
     * @param rightColor The color used for the right slice.
     * @return The generated image.
     */
    private Bitmap makeThreeColoredImage(int leftColor, int middleColor, int rightColor) {
        Bitmap image = Bitmap.createBitmap(IMAGE_SIZE_PX, IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888);
        for (int y = 0; y < IMAGE_SIZE_PX; y++) {
            for (int x = 0; x < IMAGE_SIZE_PX / 4; x++) {
                image.setPixel(x, y, leftColor);
            }
            for (int x = IMAGE_SIZE_PX / 4; x < IMAGE_SIZE_PX * 3 / 4; x++) {
                image.setPixel(x, y, middleColor);
            }
            for (int x = IMAGE_SIZE_PX * 3 / 4; x < IMAGE_SIZE_PX; x++) {
                image.setPixel(x, y, rightColor);
            }
        }
        return image;
    }

    private Bitmap makeColoredImage(int color) {
        int[] colorArray = new int[IMAGE_SIZE_PX * IMAGE_SIZE_PX];
        Arrays.fill(colorArray, color);
        return Bitmap.createBitmap(
                colorArray, IMAGE_SIZE_PX, IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888);
    }

    @Test
    @SmallTest
    public void testTwoAvatars() {
        // Make two Bitmaps that have the center part with a different color
        // +-------+------------------+--------+
        // |       |------------------|        |
        // |       |------------------|        |
        // |       |------------------|        |
        // |       |------------------|        |
        // |       |------------------|        |
        // +-------+------------------+--------+

        Bitmap avatar1 = makeThreeColoredImage(Color.RED, Color.GREEN, Color.BLUE);

        Bitmap avatar2 = makeThreeColoredImage(Color.CYAN, Color.YELLOW, Color.MAGENTA);

        // Merged Avatar should have the middle slices of both avatars, i.e. Green-Yellow.
        Bitmap mergedAvatar =
                Bitmap.createBitmap(IMAGE_SIZE_PX, IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888);
        for (int y = 0; y < IMAGE_SIZE_PX; y++) {
            for (int x = 0; x < mLeftHalfEnd; x++) {
                mergedAvatar.setPixel(x, y, Color.GREEN);
            }
            for (int x = mRightHalfStart; x < IMAGE_SIZE_PX; x++) {
                mergedAvatar.setPixel(x, y, Color.YELLOW);
            }
        }

        BitmapDrawable separateAvatarsOutput =
                (BitmapDrawable)
                        AvatarGenerator.makeRoundAvatar(
                                mResources, Arrays.asList(avatar1, avatar2), IMAGE_SIZE_PX);

        BitmapDrawable mergedAvatarOutput =
                (BitmapDrawable)
                        AvatarGenerator.makeRoundAvatar(mResources, mergedAvatar, IMAGE_SIZE_PX);

        assertTrue(separateAvatarsOutput.getBitmap().sameAs(mergedAvatarOutput.getBitmap()));
        // TODO(crbug.com/40944605): Add another test that compares the result of  makeRoundAvatar()
        // with a pre-computed Bitmap.
    }

    @Test
    @SmallTest
    public void testThreeAvatars() {
        // Make 3 Bitmaps: the first has the center part with a different color, the other two they
        // all have one color.
        // +-------+------------------+--------+
        // |       |------------------|        |
        // |       |------------------|        |
        // |       |------------------|        |
        // |       |------------------|        |
        // |       |------------------|        |
        // +-------+------------------+--------+
        Bitmap avatar1 = makeThreeColoredImage(Color.RED, Color.GREEN, Color.BLUE);

        Bitmap avatar2 = makeColoredImage(Color.CYAN);

        Bitmap avatar3 = makeColoredImage(Color.MAGENTA);

        // Merged Avatar should have the middle slices of avatar 1 (i.e. Green) and avatar 2 and 3
        // fully.
        Bitmap mergedAvatar =
                Bitmap.createBitmap(IMAGE_SIZE_PX, IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888);
        for (int y = 0; y < IMAGE_SIZE_PX; y++) {
            for (int x = 0; x < mLeftHalfEnd; x++) {
                mergedAvatar.setPixel(x, y, Color.GREEN);
            }
        }

        for (int y = 0; y < mTopHalfEnd; y++) {
            for (int x = mRightHalfStart; x < IMAGE_SIZE_PX; x++) {
                mergedAvatar.setPixel(x, y, Color.CYAN);
            }
        }

        for (int y = mBottomHalfStart; y < IMAGE_SIZE_PX; y++) {
            for (int x = mRightHalfStart; x < IMAGE_SIZE_PX; x++) {
                mergedAvatar.setPixel(x, y, Color.MAGENTA);
            }
        }

        BitmapDrawable separateAvatarsOutput =
                (BitmapDrawable)
                        AvatarGenerator.makeRoundAvatar(
                                mResources,
                                Arrays.asList(avatar1, avatar2, avatar3),
                                IMAGE_SIZE_PX);

        BitmapDrawable mergedAvatarOutput =
                (BitmapDrawable)
                        AvatarGenerator.makeRoundAvatar(mResources, mergedAvatar, IMAGE_SIZE_PX);

        assertTrue(separateAvatarsOutput.getBitmap().sameAs(mergedAvatarOutput.getBitmap()));
    }

    @Test
    @SmallTest
    public void testFourAvatars() {
        // Make 4 Bitmaps: each of a different color.
        Bitmap avatar1 = makeColoredImage(Color.RED);

        Bitmap avatar2 = makeColoredImage(Color.CYAN);

        Bitmap avatar3 = makeColoredImage(Color.MAGENTA);

        Bitmap avatar4 = makeColoredImage(Color.YELLOW);

        // Merged Avatar should have the 4 avatar in differetn corners.
        Bitmap mergedAvatar =
                Bitmap.createBitmap(IMAGE_SIZE_PX, IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888);
        for (int y = 0; y < mTopHalfEnd; y++) {
            for (int x = 0; x < mLeftHalfEnd; x++) {
                mergedAvatar.setPixel(x, y, Color.RED);
            }
        }

        for (int y = mBottomHalfStart; y < IMAGE_SIZE_PX; y++) {
            for (int x = 0; x < mLeftHalfEnd; x++) {
                mergedAvatar.setPixel(x, y, Color.CYAN);
            }
        }

        for (int y = 0; y < mTopHalfEnd; y++) {
            for (int x = mRightHalfStart; x < IMAGE_SIZE_PX; x++) {
                mergedAvatar.setPixel(x, y, Color.MAGENTA);
            }
        }

        for (int y = mBottomHalfStart; y < IMAGE_SIZE_PX; y++) {
            for (int x = mRightHalfStart; x < IMAGE_SIZE_PX; x++) {
                mergedAvatar.setPixel(x, y, Color.YELLOW);
            }
        }

        BitmapDrawable separateAvatarsOutput =
                (BitmapDrawable)
                        AvatarGenerator.makeRoundAvatar(
                                mResources,
                                Arrays.asList(avatar1, avatar2, avatar3, avatar4),
                                IMAGE_SIZE_PX);

        BitmapDrawable mergedAvatarOutput =
                (BitmapDrawable)
                        AvatarGenerator.makeRoundAvatar(mResources, mergedAvatar, IMAGE_SIZE_PX);

        assertTrue(separateAvatarsOutput.getBitmap().sameAs(mergedAvatarOutput.getBitmap()));
    }
}
