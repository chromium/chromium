// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Tests for the {@link PlayerFrameBitmapPainter} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {PlayerFrameBitmapPainterTest.FakeShadowBitmapFactory.class})
public class PlayerFrameBitmapPainterTest {
    /** A fake {@link BitmapFactory} used to avoid native for decoding. */
    @Implements(BitmapFactory.class)
    public static class FakeShadowBitmapFactory {
        private static Map<Integer, Bitmap> sBitmaps;

        public static void setBitmaps(Map<Integer, Bitmap> bitmaps) {
            sBitmaps = bitmaps;
        }

        @Implementation
        public static Bitmap decodeByteArray(
                byte[] array, int offset, int length, BitmapFactory.Options options) {
            return sBitmaps.get(fromByteArray(array));
        }
    }

    static byte[] toByteArray(int value) {
        return new byte[] {
            (byte) (value >> 24), (byte) (value >> 16), (byte) (value >> 8), (byte) value
        };
    }

    static int fromByteArray(byte[] bytes) {
        return ((bytes[0] & 0xFF) << 24)
                | ((bytes[1] & 0xFF) << 16)
                | ((bytes[2] & 0xFF) << 8)
                | (bytes[3] & 0xFF);
    }

    /**
     * Mocks {@link Canvas} and holds all calls to {@link Canvas#drawBitmap(Bitmap, Rect, Rect,
     * Paint)}.
     */
    private class MockCanvas extends Canvas {
        private List<DrawnBitmap> mDrawnBitmaps = new ArrayList<>();

        private static class DrawnBitmap {
            private final Bitmap mBitmap;
            private final Rect mSrc;
            private final Rect mDst;

            private DrawnBitmap(Bitmap bitmap, Rect src, Rect dst) {
                mBitmap = bitmap;
                mSrc = new Rect(src);
                mDst = new Rect(dst);
            }

            @Override
            public boolean equals(Object o) {
                if (o == null) return false;

                if (this == o) return true;

                if (getClass() != o.getClass()) return false;

                DrawnBitmap od = (DrawnBitmap) o;
                return mBitmap.equals(od.mBitmap) && mSrc.equals(od.mSrc) && mDst.equals(od.mDst);
            }
        }

        @Override
        public void drawBitmap(
                @NonNull Bitmap bitmap,
                @Nullable Rect src,
                @NonNull Rect dst,
                @Nullable Paint paint) {
            mDrawnBitmaps.add(new DrawnBitmap(bitmap, src, dst));
        }

        /** Asserts if a portion of a given bitmap has been drawn on this canvas. */
        private void assertDrawBitmap(
                @NonNull Bitmap bitmap, @Nullable Rect src, @NonNull Rect dst) {
            Assert.assertTrue(
                    "Bitmap has not been drawn from " + src + " to " + dst,
                    mDrawnBitmaps.contains(new DrawnBitmap(bitmap, src, dst)));
        }

        /** Asserts the number of bitmap draw operations on this canvas. */
        private void assertNumberOfBitmapDraws(int expected) {
            Assert.assertEquals(expected, mDrawnBitmaps.size());
        }
    }

    private Bitmap[][] generateMockBitmapMatrix(int rows, int cols) {
        Bitmap[][] matrix = new Bitmap[rows][cols];
        for (int row = 0; row < matrix.length; ++row) {
            for (int col = 0; col < matrix[row].length; ++col) {
                matrix[row][col] = Mockito.mock(Bitmap.class);
            }
        }
        return matrix;
    }

    /** Verifies no draw operations are performed on the canvas if the view port is invalid. */
    @Test
    public void testDrawFaultyViewPort() {
        PlayerFrameBitmapPainter painter =
                new PlayerFrameBitmapPainter(Mockito.mock(Runnable.class), null);
        painter.updateBitmapMatrix(generateMockBitmapMatrix(2, 3));
        painter.updateTileDimensions(new Size(10, -5));
        painter.updateViewPort(0, 5, 10, -10);

        MockCanvas canvas = new MockCanvas();
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(0);

        // Update the view port so it is covered by 2 bitmap tiles.
        painter.updateTileDimensions(new Size(10, 10));
        painter.updateViewPort(0, 5, 10, 15);
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(2);
    }

    /** Verifies no draw operations are performed on the canvas if the bitmap matrix is invalid. */
    @Test
    public void testDrawFaultyBitmapMatrix() {
        PlayerFrameBitmapPainter painter =
                new PlayerFrameBitmapPainter(Mockito.mock(Runnable.class), null);
        painter.updateBitmapMatrix(new Bitmap[0][0]);
        // This view port is covered by 2 bitmap tiles, so there should be 2 draw operations on
        // the canvas.
        painter.updateTileDimensions(new Size(10, 10));
        painter.updateViewPort(0, 5, 10, 15);

        MockCanvas canvas = new MockCanvas();
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(0);

        painter.updateBitmapMatrix(generateMockBitmapMatrix(2, 1));
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(2);
    }

    /**
     * Verified {@link PlayerFrameBitmapPainter#onDraw} draws the right bitmap tiles, at the correct
     * coordinates, for the given view port.
     */
    @Test
    public void testDraw() {
        Runnable invalidator = Mockito.mock(Runnable.class);
        PlayerFrameBitmapPainter painter = new PlayerFrameBitmapPainter(invalidator, null);

        // Prepare the bitmap matrix.
        Bitmap[][] bitmaps = new Bitmap[2][2];
        Bitmap bitmap00 = Mockito.mock(Bitmap.class);
        Bitmap bitmap10 = Mockito.mock(Bitmap.class);
        Bitmap bitmap01 = Mockito.mock(Bitmap.class);
        Bitmap bitmap11 = Mockito.mock(Bitmap.class);
        bitmaps[0][0] = bitmap00;
        bitmaps[1][0] = bitmap10;
        bitmaps[0][1] = bitmap01;
        bitmaps[1][1] = bitmap11;

        painter.updateBitmapMatrix(bitmaps);
        painter.updateTileDimensions(new Size(10, 15));
        painter.updateViewPort(5, 10, 15, 25);

        // Make sure the invalidator was called after updating the bitmap matrix and the view port.
        verify(invalidator, times(2)).run();

        MockCanvas canvas = new MockCanvas();
        painter.onDraw(canvas);
        // Verify that the correct portions of each bitmap tiles is painted in the correct
        // positions of in the canvas.
        canvas.assertNumberOfBitmapDraws(4);
        canvas.assertDrawBitmap(bitmap00, new Rect(5, 10, 10, 15), new Rect(0, 0, 5, 5));
        canvas.assertDrawBitmap(bitmap10, new Rect(5, 0, 10, 10), new Rect(0, 5, 5, 15));
        canvas.assertDrawBitmap(bitmap01, new Rect(0, 10, 5, 15), new Rect(5, 0, 10, 5));
        canvas.assertDrawBitmap(bitmap11, new Rect(0, 0, 5, 10), new Rect(5, 5, 10, 15));

        painter.destroy();
    }

    /**
     * Tests that first paint callback is called on the first paint operation, and the first paint
     * operation only.
     */
    @Test
    public void testFirstPaintListener() {
        Runnable invalidator = Mockito.mock(Runnable.class);
        CallbackHelper firstPaintCallback = new CallbackHelper();
        PlayerFrameBitmapPainter painter =
                new PlayerFrameBitmapPainter(invalidator, firstPaintCallback::notifyCalled);
        MockCanvas canvas = new MockCanvas();

        // Prepare the bitmap matrix.
        Bitmap[][] bitmaps = new Bitmap[1][1];
        bitmaps[0][0] = Mockito.mock(Bitmap.class);

        painter.updateBitmapMatrix(bitmaps);
        painter.updateTileDimensions(new Size(10, 15));
        painter.updateViewPort(5, 10, 15, 25);

        Assert.assertEquals(
                "First paint listener shouldn't have been called",
                0,
                firstPaintCallback.getCallCount());

        painter.onDraw(canvas);
        Assert.assertEquals(
                "First paint listener should have been called",
                1,
                firstPaintCallback.getCallCount());

        painter.onDraw(canvas);
        Assert.assertEquals(
                "First paint listener should have been called only once",
                1,
                firstPaintCallback.getCallCount());
    }
}
