// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;
import static org.robolectric.annotation.LooperMode.Mode.PAUSED;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Looper;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests for the {@link PlayerFrameBitmapPainter} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(PAUSED)
@Config(shadows = {PlayerFrameBitmapPainterTest.FakeShadowBitmapFactory.class})
public class PlayerFrameBitmapPainterTest {
    /**
     * A fake {@link BitmapFactory} used to avoid native for decoding.
     */
    @Implements(BitmapFactory.class)
    public static class FakeShadowBitmapFactory {
        private static Map<Integer, Bitmap> sBitmaps;

        public static void setBitmaps(Map<Integer, Bitmap> bitmaps) {
            sBitmaps = bitmaps;
        }

        @Implementation
        public static Bitmap decodeByteArray(byte[] array, int offset, int length) {
            return sBitmaps.get(fromByteArray(array));
        }
    }

    static byte[] toByteArray(int value) {
        return new byte[] {
                (byte) (value >> 24), (byte) (value >> 16), (byte) (value >> 8), (byte) (value)};
    }

    static int fromByteArray(byte[] bytes) {
        return ((bytes[0] & 0xFF) << 24) | ((bytes[1] & 0xFF) << 16) | ((bytes[2] & 0xFF) << 8)
                | ((bytes[3] & 0xFF));
    }

    /**
     * Mocks {@link Canvas} and holds all calls to
     * {@link Canvas#drawBitmap(Bitmap, Rect, Rect, Paint)}.
     */
    private class MockCanvas extends Canvas {
        private List<DrawnBitmap> mDrawnBitmaps = new ArrayList<>();

        private class DrawnBitmap {
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
        public void drawBitmap(@NonNull Bitmap bitmap, @Nullable Rect src, @NonNull Rect dst,
                @Nullable Paint paint) {
            mDrawnBitmaps.add(new DrawnBitmap(bitmap, src, dst));
        }

        /**
         * Asserts if a portion of a given bitmap has been drawn on this canvas.
         */
        private void assertDrawBitmap(
                @NonNull Bitmap bitmap, @Nullable Rect src, @NonNull Rect dst) {
            Assert.assertTrue(bitmap + " has not been drawn from " + src + " to " + dst,
                    mDrawnBitmaps.contains(new DrawnBitmap(bitmap, src, dst)));
        }

        /**
         * Asserts the number of bitmap draw operations on this canvas.
         */
        private void assertNumberOfBitmapDraws(int expected) {
            Assert.assertEquals(expected, mDrawnBitmaps.size());
        }
    }

    private CompressibleBitmap[][] generateMockBitmapMatrix(int rows, int cols) {
        CompressibleBitmap[][] matrix = new CompressibleBitmap[rows][cols];
        for (int row = 0; row < matrix.length; ++row) {
            for (int col = 0; col < matrix[row].length; ++col) {
                matrix[row][col] = Mockito.mock(CompressibleBitmap.class);
                when(matrix[row][col].getBitmap()).thenReturn(Mockito.mock(Bitmap.class));
                when(matrix[row][col].lock()).thenReturn(true);
            }
        }
        return matrix;
    }

    /**
     * Verifies no draw operations are performed on the canvas if the view port is invalid.
     */
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

    /**
     * Verifies no draw operations are performed on the canvas if the bitmap matrix is invalid.
     */
    @Test
    public void testDrawFaultyBitmapMatrix() {
        PlayerFrameBitmapPainter painter =
                new PlayerFrameBitmapPainter(Mockito.mock(Runnable.class), null);
        painter.updateBitmapMatrix(new CompressibleBitmap[0][0]);
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
    public void testDrawNoDecode() {
        Runnable invalidator = Mockito.mock(Runnable.class);
        PlayerFrameBitmapPainter painter = new PlayerFrameBitmapPainter(invalidator, null);

        // Prepare the bitmap matrix.
        CompressibleBitmap[][] bitmaps = new CompressibleBitmap[2][2];
        CompressibleBitmap compressibleBitmap00 = Mockito.mock(CompressibleBitmap.class);
        CompressibleBitmap compressibleBitmap10 = Mockito.mock(CompressibleBitmap.class);
        CompressibleBitmap compressibleBitmap01 = Mockito.mock(CompressibleBitmap.class);
        CompressibleBitmap compressibleBitmap11 = Mockito.mock(CompressibleBitmap.class);
        when(compressibleBitmap00.lock()).thenReturn(true);
        when(compressibleBitmap10.lock()).thenReturn(true);
        when(compressibleBitmap01.lock()).thenReturn(true);
        when(compressibleBitmap11.lock()).thenReturn(true);
        Bitmap bitmap00 = Mockito.mock(Bitmap.class);
        Bitmap bitmap10 = Mockito.mock(Bitmap.class);
        Bitmap bitmap01 = Mockito.mock(Bitmap.class);
        Bitmap bitmap11 = Mockito.mock(Bitmap.class);
        when(compressibleBitmap00.getBitmap()).thenReturn(bitmap00);
        when(compressibleBitmap10.getBitmap()).thenReturn(bitmap10);
        when(compressibleBitmap01.getBitmap()).thenReturn(bitmap01);
        when(compressibleBitmap11.getBitmap()).thenReturn(bitmap11);
        bitmaps[0][0] = compressibleBitmap00;
        bitmaps[1][0] = compressibleBitmap10;
        bitmaps[0][1] = compressibleBitmap01;
        bitmaps[1][1] = compressibleBitmap11;

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
        canvas.assertDrawBitmap(
                compressibleBitmap00.getBitmap(), new Rect(5, 10, 10, 15), new Rect(0, 0, 5, 5));
        canvas.assertDrawBitmap(
                compressibleBitmap10.getBitmap(), new Rect(5, 0, 10, 10), new Rect(0, 5, 5, 15));
        canvas.assertDrawBitmap(
                compressibleBitmap01.getBitmap(), new Rect(0, 10, 5, 15), new Rect(5, 0, 10, 5));
        canvas.assertDrawBitmap(
                compressibleBitmap11.getBitmap(), new Rect(0, 0, 5, 10), new Rect(5, 5, 10, 15));
    }

    /**
     * Verifies {@link PlayerFrameBitmapPainter#onDraw} draws the right bitmap tiles, at the correct
     * coordinates, for the given view port with compression.
     */
    @Test
    public void testDrawWithDecode() {
        Runnable invalidator = Mockito.mock(Runnable.class);
        PlayerFrameBitmapPainter painter = new PlayerFrameBitmapPainter(invalidator, null);

        SequencedTaskRunner taskRunner = Mockito.mock(SequencedTaskRunner.class);
        doAnswer(invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(taskRunner)
                .postTask(any());

        // Prepare the bitmap matrix.
        Bitmap bitmap00 = Mockito.mock(Bitmap.class);
        Bitmap bitmap10 = Mockito.mock(Bitmap.class);
        Bitmap bitmap01 = Mockito.mock(Bitmap.class);
        Bitmap bitmap11 = Mockito.mock(Bitmap.class);
        Bitmap bitmap20 = Mockito.mock(Bitmap.class);
        Bitmap bitmap21 = Mockito.mock(Bitmap.class);
        Map<Integer, Bitmap> bitmapMap = new HashMap<>();
        bitmapMap.put(bitmap00.hashCode(), bitmap00);
        bitmapMap.put(bitmap10.hashCode(), bitmap10);
        bitmapMap.put(bitmap01.hashCode(), bitmap01);
        bitmapMap.put(bitmap11.hashCode(), bitmap11);
        bitmapMap.put(bitmap20.hashCode(), bitmap20);
        bitmapMap.put(bitmap21.hashCode(), bitmap21);
        for (Bitmap bitmap : bitmapMap.values()) {
            when(bitmap.compress(any(), anyInt(), any())).thenAnswer(invocation -> {
                ((ByteArrayOutputStream) invocation.getArgument(2))
                        .write(toByteArray(bitmap.hashCode()), 0, 4);
                return true;
            });
        }
        FakeShadowBitmapFactory.setBitmaps(bitmapMap);
        CompressibleBitmap[][] bitmaps = new CompressibleBitmap[3][2];
        CompressibleBitmap compressibleBitmap00 =
                new CompressibleBitmap(bitmap00, taskRunner, false);
        CompressibleBitmap compressibleBitmap10 =
                new CompressibleBitmap(bitmap10, taskRunner, false);
        CompressibleBitmap compressibleBitmap01 =
                new CompressibleBitmap(bitmap01, taskRunner, false);
        CompressibleBitmap compressibleBitmap11 =
                new CompressibleBitmap(bitmap11, taskRunner, false);
        CompressibleBitmap compressibleBitmap20 =
                new CompressibleBitmap(bitmap20, taskRunner, false);
        CompressibleBitmap compressibleBitmap21 =
                new CompressibleBitmap(bitmap21, taskRunner, false);
        bitmaps[0][0] = compressibleBitmap00;
        bitmaps[1][0] = compressibleBitmap10;
        bitmaps[0][1] = compressibleBitmap01;
        bitmaps[1][1] = compressibleBitmap11;
        bitmaps[2][0] = compressibleBitmap20;
        bitmaps[2][1] = compressibleBitmap21;

        painter.updateBitmapMatrix(bitmaps);
        painter.updateTileDimensions(new Size(10, 15));
        painter.updateViewPort(0, 0, 20, 30);

        MockCanvas canvas = new MockCanvas();
        // Make sure the invalidator was called after updating the bitmap matrix and the view port.
        InOrder inOrder = inOrder(invalidator);
        inOrder.verify(invalidator, times(2)).run();

        // Draw once to force decode.
        painter.onDraw(canvas);
        shadowOf(Looper.getMainLooper()).idle();
        inOrder.verify(invalidator, times(1)).run();
        // Now draw everything.
        painter.onDraw(canvas);
        // Verify that the correct portions of each bitmap tiles is painted in the correct
        // positions of in the canvas.
        canvas.assertNumberOfBitmapDraws(4);
        canvas.assertDrawBitmap(
                compressibleBitmap00.getBitmap(), new Rect(0, 0, 10, 15), new Rect(0, 0, 10, 15));
        canvas.assertDrawBitmap(
                compressibleBitmap10.getBitmap(), new Rect(0, 0, 10, 15), new Rect(0, 15, 10, 30));
        canvas.assertDrawBitmap(
                compressibleBitmap01.getBitmap(), new Rect(0, 0, 10, 15), new Rect(10, 0, 20, 15));
        canvas.assertDrawBitmap(
                compressibleBitmap11.getBitmap(), new Rect(0, 0, 10, 15), new Rect(10, 15, 20, 30));

        // Simulate scrolling to the bottom only half the tiles will be drawn until the others are
        // inflated.
        canvas = new MockCanvas();
        painter.updateViewPort(0, 15, 20, 45);
        inOrder.verify(invalidator, times(1)).run();

        painter.onDraw(canvas);
        shadowOf(Looper.getMainLooper()).idle();
        inOrder.verify(invalidator, times(1)).run();
        canvas.assertNumberOfBitmapDraws(2);
        canvas.assertDrawBitmap(
                compressibleBitmap10.getBitmap(), new Rect(0, 0, 10, 15), new Rect(0, 0, 10, 15));
        canvas.assertDrawBitmap(
                compressibleBitmap11.getBitmap(), new Rect(0, 0, 10, 15), new Rect(10, 0, 20, 15));

        // Now that the bitmaps are decoded another draw can occur with all of them.
        canvas = new MockCanvas();
        painter.onDraw(canvas);
        canvas.assertNumberOfBitmapDraws(4);
        canvas.assertDrawBitmap(
                compressibleBitmap10.getBitmap(), new Rect(0, 0, 10, 15), new Rect(0, 0, 10, 15));
        canvas.assertDrawBitmap(
                compressibleBitmap20.getBitmap(), new Rect(0, 0, 10, 15), new Rect(0, 15, 10, 30));
        canvas.assertDrawBitmap(
                compressibleBitmap11.getBitmap(), new Rect(0, 0, 10, 15), new Rect(10, 0, 20, 15));
        canvas.assertDrawBitmap(
                compressibleBitmap21.getBitmap(), new Rect(0, 0, 10, 15), new Rect(10, 15, 20, 30));

        Assert.assertNull(compressibleBitmap00.getBitmap());
        Assert.assertNull(compressibleBitmap01.getBitmap());
    }

    /**
     * Tests that first paint callback is called on the first paint operation, and the first paint
     * operation only.
     */
    @Test
    public void testFirstPaintListener() {
        Runnable invalidator = Mockito.mock(Runnable.class);
        CallbackHelper firstPaintCallback = new CallbackHelper();
        PlayerFrameBitmapPainter painter = new PlayerFrameBitmapPainter(invalidator,
                firstPaintCallback::notifyCalled);
        MockCanvas canvas = new MockCanvas();

        // Prepare the bitmap matrix.
        CompressibleBitmap[][] bitmaps = new CompressibleBitmap[1][1];
        bitmaps[0][0] = Mockito.mock(CompressibleBitmap.class);
        when(bitmaps[0][0].getBitmap()).thenReturn(Mockito.mock(Bitmap.class));
        when(bitmaps[0][0].lock()).thenReturn(true);

        painter.updateBitmapMatrix(bitmaps);
        painter.updateTileDimensions(new Size(10, 15));
        painter.updateViewPort(5, 10, 15, 25);

        Assert.assertEquals("First paint listener shouldn't have been called", 0,
                firstPaintCallback.getCallCount());

        painter.onDraw(canvas);
        Assert.assertEquals("First paint listener should have been called", 1,
                firstPaintCallback.getCallCount());

        painter.onDraw(canvas);
        Assert.assertEquals("First paint listener should have been called only once", 1,
                firstPaintCallback.getCallCount());
    }
}
