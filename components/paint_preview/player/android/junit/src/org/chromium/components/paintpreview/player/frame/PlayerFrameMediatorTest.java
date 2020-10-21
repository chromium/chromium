// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.inOrder;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Parcel;
import android.util.Pair;
import android.util.Size;
import android.view.View;
import android.widget.OverScroller;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.UnguessableToken;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.components.paintpreview.player.PlayerGestureListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for the {@link PlayerFrameMediator} class. This also serves as a sort of integration test
 * for the {@link PlayerFrameScrollController}, {@link PlayerFrameScaleController},
 * {@link PlayerFrameViewport}, and {@link PlayerFrameBitmapState}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {PaintPreviewCustomFlingingShadowScroller.class, ShadowView.class})
public class PlayerFrameMediatorTest {
    private static final int CONTENT_WIDTH = 560;
    private static final int CONTENT_HEIGHT = 1150;

    private UnguessableToken mFrameGuid;
    private PropertyModel mModel;
    private TestPlayerCompositorDelegate mCompositorDelegate;
    private OverScroller mScroller;
    private boolean mHasUserInteraction;
    private PlayerGestureListener mGestureListener;
    private PlayerFrameMediator mMediator;
    private PlayerFrameBitmapStateController mBitmapStateController;
    private PlayerFrameScrollController mScrollController;
    private PlayerFrameScaleController mScaleController;

    /**
     * Generate an UnguessableToken with a static value.
     */
    private UnguessableToken frameGuid() {
        // Use a parcel for testing to avoid calling the normal native constructor.
        Parcel parcel = Parcel.obtain();
        parcel.writeLong(123321L);
        parcel.writeLong(987654L);
        parcel.setDataPosition(0);
        return UnguessableToken.CREATOR.createFromParcel(parcel);
    }

    /**
     * Gets the visible bitmap state from the bitmap state controller.
     */
    private PlayerFrameBitmapState getVisibleBitmapState() {
        return mBitmapStateController.getBitmapState(false);
    }

    /**
     * Used for keeping track of all bitmap requests that {@link PlayerFrameMediator} makes.
     */
    private class RequestedBitmap {
        UnguessableToken mFrameGuid;
        Rect mClipRect;
        float mScaleFactor;
        Callback<Bitmap> mBitmapCallback;
        Runnable mErrorCallback;

        public RequestedBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
            this.mFrameGuid = frameGuid;
            this.mClipRect = clipRect;
            this.mScaleFactor = scaleFactor;
            this.mBitmapCallback = bitmapCallback;
            this.mErrorCallback = errorCallback;
        }

        public RequestedBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor) {
            this.mFrameGuid = frameGuid;
            this.mClipRect = clipRect;
            this.mScaleFactor = scaleFactor;
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) return false;

            if (o == this) return true;

            if (o.getClass() != this.getClass()) return false;

            RequestedBitmap rb = (RequestedBitmap) o;
            return rb.mClipRect.equals(mClipRect) && rb.mFrameGuid.equals(mFrameGuid)
                    && rb.mScaleFactor == mScaleFactor;
        }

        @NonNull
        @Override
        public String toString() {
            return mFrameGuid + ", " + mClipRect + ", " + mScaleFactor;
        }
    }

    /**
     * Used for keeping track of all click events that {@link PlayerFrameMediator} sends to
     * {@link PlayerCompositorDelegate}.
     */
    private class ClickedPoint {
        UnguessableToken mFrameGuid;
        int mX;
        int mY;

        public ClickedPoint(UnguessableToken frameGuid, int x, int y) {
            mFrameGuid = frameGuid;
            this.mX = x;
            this.mY = y;
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) return false;

            if (o == this) return true;

            if (o.getClass() != this.getClass()) return false;

            ClickedPoint cp = (ClickedPoint) o;
            return cp.mFrameGuid.equals(mFrameGuid) && cp.mX == mX && cp.mY == mY;
        }

        @NonNull
        @Override
        public String toString() {
            return "Click event for frame " + mFrameGuid.toString() + " on (" + mX + ", " + mY
                    + ")";
        }
    }

    /**
     * Mocks {@link PlayerCompositorDelegate}. Stores all bitmap requests as
     * {@link RequestedBitmap}s.
     */
    private class TestPlayerCompositorDelegate implements PlayerCompositorDelegate {
        List<RequestedBitmap> mRequestedBitmap = new ArrayList<>();
        List<ClickedPoint> mClickedPoints = new ArrayList<>();

        @Override
        public void requestBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
            mRequestedBitmap.add(new RequestedBitmap(
                    frameGuid, new Rect(clipRect), scaleFactor, bitmapCallback, errorCallback));
        }

        @Override
        public GURL onClick(UnguessableToken frameGuid, int x, int y) {
            mClickedPoints.add(new ClickedPoint(frameGuid, x, y));
            return null;
        }
    }

    private class MatrixMatcher implements ArgumentMatcher<Matrix> {
        private Matrix mLeft;

        MatrixMatcher(Matrix left) {
            mLeft = left;
        }

        @Override
        public boolean matches(Matrix right) {
            return mLeft.equals(right);
        }
    }

    @Before
    public void setUp() {
        mFrameGuid = frameGuid();
        mModel = new PropertyModel.Builder(PlayerFrameProperties.ALL_KEYS).build();
        mCompositorDelegate = new TestPlayerCompositorDelegate();
        mScroller = new OverScroller(ContextUtils.getApplicationContext());
        mGestureListener = new PlayerGestureListener(null, () -> mHasUserInteraction = true, null);
        Size contentSize = new Size(CONTENT_WIDTH, CONTENT_HEIGHT);
        mMediator = new PlayerFrameMediator(mModel, mCompositorDelegate, mGestureListener,
                mFrameGuid, contentSize, 0, 0);
        mScaleController =
                new PlayerFrameScaleController(mModel.get(PlayerFrameProperties.SCALE_MATRIX),
                        mMediator, mGestureListener::onScale);
        mScrollController = new PlayerFrameScrollController(
                mScroller, mMediator, mGestureListener::onScroll, mGestureListener::onFling);
        mBitmapStateController = mMediator.getBitmapStateControllerForTest();
    }

    private static Rect getRectForTile(int tileWidth, int tileHeight, int row, int col) {
        int left = col * tileWidth;
        int top = row * tileHeight;
        return new Rect(left, top, left + tileWidth, top + tileHeight);
    }

    private static List<Boolean> getVisibilities(List<View> views) {
        List<Boolean> visibilities = new ArrayList<>();
        for (View view : views) {
            visibilities.add(view.getVisibility() == View.VISIBLE);
        }
        return visibilities;
    }

    private static void assertViewportStateIs(Matrix matrix, PlayerFrameViewport viewport) {
        float matrixValues[] = new float[9];
        matrix.getValues(matrixValues);
        assert matrixValues[Matrix.MSCALE_X] == matrixValues[Matrix.MSCALE_Y];
        assertViewportStateIs(matrixValues[Matrix.MSCALE_X], matrixValues[Matrix.MTRANS_X],
                matrixValues[Matrix.MTRANS_Y], viewport);
    }

    /**
     * Asserts that the viewport's transformation state matches.
     */
    private static void assertViewportStateIs(float expectedScaleFactor, float expectedX,
            float expectedY, PlayerFrameViewport viewport) {
        final float tolerance = 0.01f;
        Assert.assertEquals(expectedScaleFactor, viewport.getScale(), tolerance);
        Assert.assertEquals(expectedX, viewport.getTransX(), tolerance);
        Assert.assertEquals(expectedY, viewport.getTransY(), tolerance);
    }

    /**
     * Tests that {@link PlayerFrameMediator} is initialized correctly on the first call to
     * {@link PlayerFrameMediator#setLayoutDimensions}.
     */
    @Test
    public void testInitialLayoutDimensions() {
        // Initial view port setup.
        mMediator.setLayoutDimensions(150, 200);

        // View port should be as big as size set in the first setLayoutDimensions call, showing
        // the top left corner.
        Rect expectedViewPort = new Rect(0, 0, 150, 200);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // The bitmap matrix should be empty, but initialized with the correct number of rows and
        // columns. Because we set the initial scale factor to view port width over content width,
        // we should have only one column.
        CompressibleBitmap[][] bitmapMatrix = mModel.get(PlayerFrameProperties.BITMAP_MATRIX);
        Assert.assertTrue(Arrays.deepEquals(bitmapMatrix, new CompressibleBitmap[4][2]));
        Assert.assertEquals(new ArrayList<Pair<View, Rect>>(),
                mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
    }

    /**
     * Tests that {@link PlayerFrameMediator} requests for the right bitmap tiles as the view port
     * moves.
     */
    @Test
    public void testBitmapRequest() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);

        // Requests for bitmaps in all tiles that are visible in the view port as well as their
        // adjacent tiles should've been made.
        // The current view port fully matches the top left bitmap tiles, so we expect requests for
        // the top left bitmaps, plus bitmaps tothe right, and below.
        // Below is a schematic of the entire bitmap matrix. Those marked with number should have
        // been requested, in the order of numbers.
        // -------------------------
        // | 1 | 3 | 6 |   |   |   |
        // -------------------------
        // | 2 | 4 | 8 |   |   |   |
        // -------------------------
        // | 5 | 7 |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        List<RequestedBitmap> expectedRequestedBitmaps = new ArrayList<>();
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 2), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        mScrollController.scrollBy(10, 20);
        // The view port was moved with the #updateViewport call. It should've been updated in the
        // model.
        Rect expectedViewPort = new Rect(10, 20, 110, 220);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        // The current viewport covers portions of the top left bitmap tiles. We have requested
        // bitmaps for 8 of them before. Make sure requests for the 4th bitmap, as well adjacent
        // bitmaps are made.
        // Below is a schematic of the entire bitmap matrix. Those marked with number should have
        // been requested, in the order of numbers.
        // -------------------------
        // | x | x | x | 3 |   |   |
        // -------------------------
        // | x | x | x | 5 |   |   |
        // -------------------------
        // | x | x | 1 | 7 |   |   |
        // -------------------------
        // | 2 | 4 | 6 |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 3), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // Move the view port slightly. It is still covered by the same tiles. Since there were
        // already bitmap requests out for those tiles and their adjacent tiles, we shouldn't have
        // made new requests.
        mScrollController.scrollBy(10, 20);
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // Move the view port to the bottom right so it covers portions of the 4 bottom right bitmap
        // tiles. New bitmap requests should be made.
        mScrollController.scrollBy(430, 900);
        expectedViewPort.set(450, 940, 550, 1140);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));

        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 9, 9), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 10, 9), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 11, 9), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 9, 10), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 10, 10), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 11, 10), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 8, 9), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 9, 8), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 10, 8), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 11, 8), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 8, 10), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 9, 11), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 10, 11), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 11, 11), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);
    }

    /**
     * Tests that the mediator keeps around the required bitmaps and removes the unrequired bitmaps
     * when the view port changes. Required bitmaps are those in the viewport and its adjacent
     * tiles.
     */
    @Test
    public void testRequiredBitmapMatrix() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);

        boolean[][] expectedRequiredBitmaps = new boolean[12][12];

        // The current view port fully matches the top left bitmap tile.
        // Below is a schematic of the entire bitmap matrix. Tiles marked with x are required for
        // the current view port.
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequiredBitmaps[0][0] = true;
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[1][0] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        mScrollController.scrollBy(10, 15);
        // The current viewport covers portions of the 4 top left bitmap tiles.
        // -------------------------
        // | x | x | x | x |   |   |
        // -------------------------
        // | x | x | x | x |   |   |
        // -------------------------
        // | x | x | x | x |   |   |
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequiredBitmaps[0][3] = true;
        expectedRequiredBitmaps[1][3] = true;
        expectedRequiredBitmaps[2][3] = true;
        expectedRequiredBitmaps[2][2] = true;
        expectedRequiredBitmaps[3][0] = true;
        expectedRequiredBitmaps[3][1] = true;
        expectedRequiredBitmaps[3][2] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        mScrollController.scrollBy(200, 400);
        // The current view port contains portions of the middle 9 tiles.
        // ---------------------
        // |   | x  | x | x |  |
        // ---------------------
        // | x | x | x | x | x |
        // ---------------------
        // | x | x | x | x | x |
        // ---------------------
        // | x | x | x | x | x |
        // ---------------------
        // |   | x | x | x |   |
        // ---------------------
        expectedRequiredBitmaps = new boolean[12][12];
        expectedRequiredBitmaps[3][4] = true;
        expectedRequiredBitmaps[3][5] = true;
        expectedRequiredBitmaps[3][6] = true;
        expectedRequiredBitmaps[4][3] = true;
        expectedRequiredBitmaps[4][4] = true;
        expectedRequiredBitmaps[4][5] = true;
        expectedRequiredBitmaps[4][6] = true;
        expectedRequiredBitmaps[4][7] = true;
        expectedRequiredBitmaps[5][3] = true;
        expectedRequiredBitmaps[5][4] = true;
        expectedRequiredBitmaps[5][5] = true;
        expectedRequiredBitmaps[5][6] = true;
        expectedRequiredBitmaps[5][7] = true;
        expectedRequiredBitmaps[6][3] = true;
        expectedRequiredBitmaps[6][4] = true;
        expectedRequiredBitmaps[6][5] = true;
        expectedRequiredBitmaps[6][6] = true;
        expectedRequiredBitmaps[6][7] = true;
        expectedRequiredBitmaps[7][4] = true;
        expectedRequiredBitmaps[7][5] = true;
        expectedRequiredBitmaps[7][6] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        mScrollController.scrollBy(200, 400);
        // The current view port contains portions of the 9 bottom right tiles.
        // Tiles marked with x are required for the current view port.
        expectedRequiredBitmaps = new boolean[12][12];
        expectedRequiredBitmaps[7][8] = true;
        expectedRequiredBitmaps[7][9] = true;
        expectedRequiredBitmaps[7][10] = true;
        expectedRequiredBitmaps[8][7] = true;
        expectedRequiredBitmaps[8][8] = true;
        expectedRequiredBitmaps[8][9] = true;
        expectedRequiredBitmaps[8][10] = true;
        expectedRequiredBitmaps[8][11] = true;
        expectedRequiredBitmaps[9][7] = true;
        expectedRequiredBitmaps[9][8] = true;
        expectedRequiredBitmaps[9][9] = true;
        expectedRequiredBitmaps[9][10] = true;
        expectedRequiredBitmaps[9][11] = true;
        expectedRequiredBitmaps[10][7] = true;
        expectedRequiredBitmaps[10][8] = true;
        expectedRequiredBitmaps[10][9] = true;
        expectedRequiredBitmaps[10][10] = true;
        expectedRequiredBitmaps[10][11] = true;
        expectedRequiredBitmaps[11][8] = true;
        expectedRequiredBitmaps[11][9] = true;
        expectedRequiredBitmaps[11][10] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));
    }

    /**
     * Mocks responses on bitmap requests from {@link PlayerFrameMediator} and tests those responses
     * are correctly handled.
     */
    @Test
    public void testBitmapRequestResponse() {
        // Sets the bitmap tile size to 150x200 and triggers bitmap request for the upper left tiles
        // and their adjacent tiles.
        mMediator.updateViewportSize(150, 200, 1f);

        // Create mock bitmaps for response.
        Bitmap bitmap00 = Mockito.mock(Bitmap.class);
        Bitmap bitmap10 = Mockito.mock(Bitmap.class);
        Bitmap bitmap20 = Mockito.mock(Bitmap.class);
        Bitmap bitmap01 = Mockito.mock(Bitmap.class);
        Bitmap bitmap11 = Mockito.mock(Bitmap.class);
        Bitmap bitmap21 = Mockito.mock(Bitmap.class);
        Bitmap bitmap31 = Mockito.mock(Bitmap.class);
        Bitmap bitmap02 = Mockito.mock(Bitmap.class);
        Bitmap bitmap12 = Mockito.mock(Bitmap.class);
        Bitmap bitmap22 = Mockito.mock(Bitmap.class);
        Bitmap bitmap32 = Mockito.mock(Bitmap.class);
        Bitmap bitmap03 = Mockito.mock(Bitmap.class);
        Bitmap bitmap13 = Mockito.mock(Bitmap.class);
        Bitmap bitmap23 = Mockito.mock(Bitmap.class);
        SequencedTaskRunner mockTaskRunner = Mockito.mock(SequencedTaskRunner.class);
        CompressibleBitmap compressibleBitmap00 =
                new CompressibleBitmap(bitmap00, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap10 =
                new CompressibleBitmap(bitmap10, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap20 =
                new CompressibleBitmap(bitmap20, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap01 =
                new CompressibleBitmap(bitmap01, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap11 =
                new CompressibleBitmap(bitmap11, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap21 =
                new CompressibleBitmap(bitmap21, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap31 =
                new CompressibleBitmap(bitmap31, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap02 =
                new CompressibleBitmap(bitmap02, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap12 =
                new CompressibleBitmap(bitmap12, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap22 =
                new CompressibleBitmap(bitmap22, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap32 =
                new CompressibleBitmap(bitmap32, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap03 =
                new CompressibleBitmap(bitmap03, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap13 =
                new CompressibleBitmap(bitmap13, mockTaskRunner, true);
        CompressibleBitmap compressibleBitmap23 =
                new CompressibleBitmap(bitmap23, mockTaskRunner, true);

        CompressibleBitmap[][] expectedBitmapMatrix = new CompressibleBitmap[12][8];
        expectedBitmapMatrix[0][0] = compressibleBitmap00;
        expectedBitmapMatrix[0][1] = compressibleBitmap01;
        expectedBitmapMatrix[0][2] = compressibleBitmap02;
        expectedBitmapMatrix[1][0] = compressibleBitmap10;
        expectedBitmapMatrix[1][1] = compressibleBitmap11;
        expectedBitmapMatrix[1][2] = compressibleBitmap12;
        expectedBitmapMatrix[2][0] = compressibleBitmap20;
        expectedBitmapMatrix[2][1] = compressibleBitmap21;

        // Call the request callback with mock bitmaps and assert they're added to the model.
        mCompositorDelegate.mRequestedBitmap.get(0).mBitmapCallback.onResult(
                compressibleBitmap00.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(1).mBitmapCallback.onResult(
                compressibleBitmap10.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(2).mBitmapCallback.onResult(
                compressibleBitmap01.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(3).mBitmapCallback.onResult(
                compressibleBitmap11.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(4).mBitmapCallback.onResult(
                compressibleBitmap20.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(5).mBitmapCallback.onResult(
                compressibleBitmap02.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(6).mBitmapCallback.onResult(
                compressibleBitmap21.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(7).mBitmapCallback.onResult(
                compressibleBitmap12.getBitmap());
        CompressibleBitmap[][] mat = mModel.get(PlayerFrameProperties.BITMAP_MATRIX);
        Assert.assertTrue(Arrays.deepEquals(
                expectedBitmapMatrix, mModel.get(PlayerFrameProperties.BITMAP_MATRIX)));

        // Move the viewport slightly..
        mScrollController.scrollBy(10, 10);

        // Scroll should've triggered bitmap requests for an the new tiles as well as adjacent
        // tiles. See comments on {@link #testBitmapRequest} for details on which tiles will be
        // requested.
        // Call the request callback with mock bitmaps and assert they're added to the model.
        expectedBitmapMatrix[2][2] = compressibleBitmap22;
        expectedBitmapMatrix[0][3] = compressibleBitmap03;
        expectedBitmapMatrix[3][1] = compressibleBitmap31;
        expectedBitmapMatrix[1][3] = compressibleBitmap13;
        expectedBitmapMatrix[3][2] = compressibleBitmap32;
        expectedBitmapMatrix[2][3] = compressibleBitmap23;
        mCompositorDelegate.mRequestedBitmap.get(8).mBitmapCallback.onResult(
                compressibleBitmap22.getBitmap());
        // Mock a compositing failure for this tile. No bitmaps should be added.
        mCompositorDelegate.mRequestedBitmap.get(9).mErrorCallback.run();
        mCompositorDelegate.mRequestedBitmap.get(10).mBitmapCallback.onResult(
                compressibleBitmap31.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(11).mBitmapCallback.onResult(
                compressibleBitmap03.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(12).mBitmapCallback.onResult(
                compressibleBitmap13.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(13).mBitmapCallback.onResult(
                compressibleBitmap32.getBitmap());
        mCompositorDelegate.mRequestedBitmap.get(14).mBitmapCallback.onResult(
                compressibleBitmap23.getBitmap());
        Assert.assertTrue(Arrays.deepEquals(
                expectedBitmapMatrix, mModel.get(PlayerFrameProperties.BITMAP_MATRIX)));

        // Assert 15 bitmap requests have been made in total.
        Assert.assertEquals(15, mCompositorDelegate.mRequestedBitmap.size());

        // Move the view port while staying within the current tiles in order to trigger the
        // request logic again. Make sure only one new request is added, for the tile with a
        // compositing failure.
        mScrollController.scrollBy(10, 10);
        Assert.assertEquals(16, mCompositorDelegate.mRequestedBitmap.size());
        Assert.assertEquals(new RequestedBitmap(mFrameGuid, getRectForTile(75, 100, 3, 0), 1f),
                mCompositorDelegate.mRequestedBitmap.get(
                        mCompositorDelegate.mRequestedBitmap.size() - 1));
    }

    /**
     * View port should be updated on scroll events. Bounds checks are verified in
     * {@link PlayerFrameScrollControllerTest}.
     */
    @Test
    public void testViewPortOnScrollBy() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        Rect expectedViewPort = new Rect(0, 0, 100, 200);

        // Scroll right and down by a within bounds amount. Both scroll directions should be
        // effective.
        Assert.assertTrue(mScrollController.scrollBy(250f, 80f));
        expectedViewPort.offset(250, 80);
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));
    }

    /**
     * Tests sub-frames' visibility when view port changes. sub-frames that are out of the view
     * port's bounds should not be added to the model.
     */
    @Test
    public void testSubFramesPosition() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        View subFrame1View = new View(context);
        View subFrame2View = new View(context);
        View subFrame3View = new View(context);

        Pair<View, Rect> subFrame1 = new Pair<>(subFrame1View, new Rect(10, 20, 60, 120));
        Pair<View, Rect> subFrame2 = new Pair<>(subFrame2View, new Rect(30, 130, 70, 160));
        Pair<View, Rect> subFrame3 = new Pair<>(subFrame3View, new Rect(120, 35, 150, 65));

        mMediator.addSubFrame(
                subFrame1.first, subFrame1.second, Mockito.mock(PlayerFrameMediator.class));
        mMediator.addSubFrame(
                subFrame2.first, subFrame2.second, Mockito.mock(PlayerFrameMediator.class));
        mMediator.addSubFrame(
                subFrame3.first, subFrame3.second, Mockito.mock(PlayerFrameMediator.class));

        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        List<View> expectedViews = new ArrayList<>();
        List<Rect> expectedRects = new ArrayList<>();
        List<Boolean> expectedVisibility = new ArrayList<>();
        expectedViews.add(subFrame1.first);
        expectedViews.add(subFrame2.first);
        expectedViews.add(subFrame3.first);
        expectedRects.add(subFrame1.second);
        expectedRects.add(subFrame2.second);
        expectedRects.add(new Rect(0, 0, 0, 0));
        expectedVisibility.add(true);
        expectedVisibility.add(true);
        expectedVisibility.add(false);
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));

        mScrollController.scrollBy(100, 0);
        expectedRects.set(0, new Rect(0, 0, 0, 0));
        expectedRects.set(1, new Rect(0, 0, 0, 0));
        expectedRects.set(2, new Rect(20, 35, 50, 65));
        expectedVisibility.clear();
        expectedVisibility.add(false);
        expectedVisibility.add(false);
        expectedVisibility.add(true);
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));

        mScrollController.scrollBy(-50, 0);
        expectedRects.clear();
        expectedRects.add(new Rect(-40, 20, 10, 120));
        expectedRects.add(new Rect(-20, 130, 20, 160));
        expectedRects.add(new Rect(70, 35, 100, 65));
        expectedVisibility.clear();
        expectedVisibility.add(true);
        expectedVisibility.add(true);
        expectedVisibility.add(true);
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));

        mScrollController.scrollBy(0, 200);
        expectedRects.clear();
        expectedRects.add(new Rect(0, 0, 0, 0));
        expectedRects.add(new Rect(0, 0, 0, 0));
        expectedRects.add(new Rect(0, 0, 0, 0));
        expectedVisibility.clear();
        expectedVisibility.add(false);
        expectedVisibility.add(false);
        expectedVisibility.add(false);
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));
    }

    /**
     * View port should be updated on fling events. There are more extensive tests for this in
     * {@link PlayerFrameScrollControllerTest}.
     */
    @Test
    public void testViewPortOnFling() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        Rect expectedViewPort = new Rect(0, 0, 100, 200);

        mScrollController.onFling(100, 0);
        expectedViewPort.offsetTo(mScroller.getFinalX(), mScroller.getFinalY());
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(expectedViewPort, mModel.get(PlayerFrameProperties.VIEWPORT));
    }

    /**
     * Tests that {@link PlayerFrameMediator} correctly relays the click events to
     * {@link PlayerCompositorDelegate} and accounts for scroll offsets.
     */
    @Test
    public void testOnClick() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);
        List<ClickedPoint> expectedClickedPoints = new ArrayList<>();

        // No scrolling has happened yet.
        mMediator.onTap(15, 26);
        expectedClickedPoints.add(new ClickedPoint(mFrameGuid, 15, 26));
        Assert.assertEquals(expectedClickedPoints, mCompositorDelegate.mClickedPoints);

        // Scroll, and then click. The call to {@link PlayerFrameMediator} must account for the
        // scroll offset.
        mScrollController.scrollBy(90, 100);
        mMediator.onTap(70, 50);
        expectedClickedPoints.add(new ClickedPoint(mFrameGuid, 160, 150));
        Assert.assertEquals(expectedClickedPoints, mCompositorDelegate.mClickedPoints);

        mScrollController.scrollBy(-40, -60);
        mMediator.onTap(30, 80);
        expectedClickedPoints.add(new ClickedPoint(mFrameGuid, 80, 120));
        Assert.assertEquals(expectedClickedPoints, mCompositorDelegate.mClickedPoints);
    }

    /**
     * Tests that {@link PlayerFrameMediator} correctly consumes scale events. There are more
     * extensive tests for keeping the viewport in bounds and ensuring limits on scaling in
     * {@link PlayerFrameScaleControllerTest}.
     */
    @Test
    public void testViewPortOnScaleBy() {
        // Initial view port setup.
        mMediator.setLayoutDimensions(100, 200);
        mMediator.updateViewportSize(100, 200, 1f);

        // The current view port fully matches the top left bitmap tile.
        // Below is a schematic of the entire bitmap matrix. Tiles marked with x are required for
        // the current view port.
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        boolean[][] expectedRequiredBitmaps = new boolean[12][12];
        expectedRequiredBitmaps[0][0] = true;
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[1][0] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        mBitmapStateController.swapForTest();
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        // Now a scale factor of 2 will be applied. This will happen at a focal point of 0, 0.
        // The same bitmaps will be required but the grid will be double the size.
        Assert.assertTrue(mScaleController.scaleBy(2f, 0, 0));
        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        mBitmapStateController.swapForTest();

        expectedRequiredBitmaps = new boolean[23][23];
        expectedRequiredBitmaps[0][0] = true;
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[1][0] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        // Reduce the scale factor by 0.5 returning to a scale of 1.
        Assert.assertTrue(mScaleController.scaleBy(0.5f, 0, 0));
        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        mBitmapStateController.swapForTest();

        expectedRequiredBitmaps = new boolean[12][12];
        expectedRequiredBitmaps[0][0] = true;
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[1][0] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));
    }

    /**
     * Tests that {@link PlayerFrameMediator} works correctly when scrolling.
     */
    @Test
    public void testViewPortOnScaleByWithScroll() {
        // Initial view port setup.
        mMediator.updateViewportSize(100, 200, 1f);

        boolean[][] expectedRequiredBitmaps = new boolean[12][12];

        // STEP 1: Original request.
        // The current view port fully matches the top left bitmap tile.
        // Below is a schematic of the entire bitmap matrix. Tiles marked with x are required for
        // the current view port.
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // | x | x |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        expectedRequiredBitmaps[0][0] = true;
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[1][0] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        List<RequestedBitmap> expectedRequestedBitmaps = new ArrayList<>();
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 2), 1f));

        // Both matricies should be identity to start.
        assertViewportStateIs(1f, 0f, 0f, mMediator.getViewport());
        Assert.assertTrue(mModel.get(PlayerFrameProperties.SCALE_MATRIX).isIdentity());
        // Ensure the correct bitmaps are required and requested.
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // STEP 2: Scroll slightly.
        mScrollController.scrollBy(10, 15);
        // -------------------------
        // | x | x | x | x |   |   |
        // -------------------------
        // | x | x | x | x |   |   |
        // -------------------------
        // | x | x | x | x |   |   |
        // -------------------------
        // | x | x | x |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        expectedRequiredBitmaps[0][3] = true;
        expectedRequiredBitmaps[1][3] = true;
        expectedRequiredBitmaps[2][2] = true;
        expectedRequiredBitmaps[2][3] = true;
        expectedRequiredBitmaps[3][0] = true;
        expectedRequiredBitmaps[3][1] = true;
        expectedRequiredBitmaps[3][2] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 3), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // The viewport matrix should track scroll and zoom.
        Matrix expectedViewportMatrix = new Matrix();
        float[] expectedViewportMatrixValues = new float[9];
        expectedViewportMatrix.getValues(expectedViewportMatrixValues);
        expectedViewportMatrixValues[Matrix.MTRANS_X] = 10;
        expectedViewportMatrixValues[Matrix.MTRANS_Y] = 15;
        expectedViewportMatrix.setValues(expectedViewportMatrixValues);

        assertViewportStateIs(expectedViewportMatrix, mMediator.getViewport());
        Assert.assertTrue(mModel.get(PlayerFrameProperties.SCALE_MATRIX).isIdentity());

        // STEP 3: Now a scale factor of 2 will be applied. This will happen at a focal point of 50,
        // 100.
        Assert.assertTrue(mScaleController.scaleBy(2f, 50f, 100f));

        // Before the scaling commits both matricies should update.
        expectedViewportMatrix.postScale(2f, 2f, -50f, -100f);
        Matrix expectedBitmapMatrix = new Matrix();
        expectedBitmapMatrix.postScale(2f, 2f, 50f, 100f);
        assertViewportStateIs(expectedViewportMatrix, mMediator.getViewport());
        Assert.assertEquals(expectedBitmapMatrix, mModel.get(PlayerFrameProperties.SCALE_MATRIX));

        // Bitmaps should be the same as before scaling until scaling is finished.
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);
        mCompositorDelegate.mRequestedBitmap.clear();
        expectedRequestedBitmaps.clear();

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        mBitmapStateController.swapForTest();

        expectedRequiredBitmaps = new boolean[23][23];
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[0][3] = true;
        expectedRequiredBitmaps[1][0] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[1][3] = true;
        expectedRequiredBitmaps[1][4] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        expectedRequiredBitmaps[2][2] = true;
        expectedRequiredBitmaps[2][3] = true;
        expectedRequiredBitmaps[2][4] = true;
        expectedRequiredBitmaps[3][0] = true;
        expectedRequiredBitmaps[3][1] = true;
        expectedRequiredBitmaps[3][2] = true;
        expectedRequiredBitmaps[3][3] = true;
        expectedRequiredBitmaps[3][4] = true;
        expectedRequiredBitmaps[4][1] = true;
        expectedRequiredBitmaps[4][2] = true;
        expectedRequiredBitmaps[4][3] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 0), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 0), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 0), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 4), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 4), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 4), 2f));

        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        // The bitmap matrix should be cleared.
        expectedBitmapMatrix.reset();
        Assert.assertTrue(mModel.get(PlayerFrameProperties.SCALE_MATRIX).isIdentity());

        // STEP4: Now a scale factor of 0.5 will be applied. This will happen at a focal point of
        // 50, 100.
        Assert.assertTrue(mScaleController.scaleBy(0.5f, 50f, 100f));

        // Ensure the matricies are correct mid-scale.
        expectedViewportMatrix.postScale(0.5f, 0.5f, -50f, -100f);
        expectedBitmapMatrix.postScale(0.5f, 0.5f, 50f, 100f);
        assertViewportStateIs(expectedViewportMatrix, mMediator.getViewport());
        Assert.assertEquals(expectedBitmapMatrix, mModel.get(PlayerFrameProperties.SCALE_MATRIX));

        // Bitmaps should be the same as before scaling until scaling is finished.
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);
        mCompositorDelegate.mRequestedBitmap.clear();
        expectedRequestedBitmaps.clear();

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        mBitmapStateController.swapForTest();

        expectedRequiredBitmaps = new boolean[12][12];
        expectedRequiredBitmaps[0][0] = true;
        expectedRequiredBitmaps[0][1] = true;
        expectedRequiredBitmaps[0][2] = true;
        expectedRequiredBitmaps[0][3] = true;
        expectedRequiredBitmaps[1][0] = true;
        expectedRequiredBitmaps[1][1] = true;
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[1][3] = true;
        expectedRequiredBitmaps[2][0] = true;
        expectedRequiredBitmaps[2][1] = true;
        expectedRequiredBitmaps[2][2] = true;
        expectedRequiredBitmaps[2][3] = true;
        expectedRequiredBitmaps[3][0] = true;
        expectedRequiredBitmaps[3][1] = true;
        expectedRequiredBitmaps[3][2] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 0), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 1), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 0, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 3), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 2), 1f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 3), 1f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        expectedBitmapMatrix.reset();
        Assert.assertTrue(mModel.get(PlayerFrameProperties.SCALE_MATRIX).isIdentity());

        // Now a scale factor of 2 will be applied. This will happen at a focal point of 100, 200.
        // Due to the position of the focal point the required bitmaps will move.
        // -------------------------
        // |   | x | x | x |   |   |
        // -------------------------
        // | x | x | x | x | x |   |
        // -------------------------
        // | x | x | x | x | x |   |
        // -------------------------
        // | x | x | x | x | x |   |
        // -------------------------
        // |   | x | x | x |   |   |
        // -------------------------
        // |   |   |   |   |   |   |
        Assert.assertTrue(mScaleController.scaleBy(2f, 100f, 200f));

        expectedViewportMatrix.postScale(2f, 2f, -100f, -200f);
        expectedBitmapMatrix.postScale(2f, 2f, 100f, 200f);
        assertViewportStateIs(expectedViewportMatrix, mMediator.getViewport());
        Assert.assertEquals(expectedBitmapMatrix, mModel.get(PlayerFrameProperties.SCALE_MATRIX));

        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);
        mCompositorDelegate.mRequestedBitmap.clear();
        expectedRequestedBitmaps.clear();

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        mBitmapStateController.swapForTest();

        expectedRequiredBitmaps = new boolean[23][23];
        expectedRequiredBitmaps[1][2] = true;
        expectedRequiredBitmaps[1][3] = true;
        expectedRequiredBitmaps[1][4] = true;
        expectedRequiredBitmaps[2][1] = true;
        expectedRequiredBitmaps[2][2] = true;
        expectedRequiredBitmaps[2][3] = true;
        expectedRequiredBitmaps[2][4] = true;
        expectedRequiredBitmaps[2][5] = true;
        expectedRequiredBitmaps[3][1] = true;
        expectedRequiredBitmaps[3][2] = true;
        expectedRequiredBitmaps[3][3] = true;
        expectedRequiredBitmaps[3][4] = true;
        expectedRequiredBitmaps[3][5] = true;
        expectedRequiredBitmaps[4][1] = true;
        expectedRequiredBitmaps[4][2] = true;
        expectedRequiredBitmaps[4][3] = true;
        expectedRequiredBitmaps[4][4] = true;
        expectedRequiredBitmaps[4][5] = true;
        expectedRequiredBitmaps[5][2] = true;
        expectedRequiredBitmaps[5][3] = true;
        expectedRequiredBitmaps[5][4] = true;
        Assert.assertTrue(Arrays.deepEquals(
                expectedRequiredBitmaps, getVisibleBitmapState().getRequiredBitmapsForTest()));

        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 4), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 4), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 4), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 5, 2), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 1), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 5, 3), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 1, 4), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 2, 5), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 3, 5), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 5, 4), 2f));
        expectedRequestedBitmaps.add(
                new RequestedBitmap(mFrameGuid, getRectForTile(50, 100, 4, 5), 2f));
        Assert.assertEquals(expectedRequestedBitmaps, mCompositorDelegate.mRequestedBitmap);

        expectedBitmapMatrix.reset();
        Assert.assertTrue(mModel.get(PlayerFrameProperties.SCALE_MATRIX).isIdentity());
    }

    /**
     * Tests that {@link PlayerFrameMediator} works correctly when subframes are present.
     */
    @Test
    public void testViewPortOnScaleByWithSubFrames() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        View subFrame1View = new View(context);
        View subFrame2View = new View(context);

        PlayerFrameMediator subFrame1Mediator = Mockito.mock(PlayerFrameMediator.class);
        Pair<View, Rect> subFrame1 = new Pair<>(subFrame1View, new Rect(10, 20, 60, 40));
        PlayerFrameMediator subFrame2Mediator = Mockito.mock(PlayerFrameMediator.class);
        Pair<View, Rect> subFrame2 = new Pair<>(subFrame2View, new Rect(30, 50, 70, 160));
        InOrder inOrderMediator1 = inOrder(subFrame1Mediator);
        InOrder inOrderMediator2 = inOrder(subFrame2Mediator);

        mMediator.addSubFrame(subFrame1.first, subFrame1.second, subFrame1Mediator);
        mMediator.addSubFrame(subFrame2.first, subFrame2.second, subFrame2Mediator);

        // Both subframes should be visible.
        mMediator.updateViewportSize(50, 100, 1f);
        List<View> expectedViews = new ArrayList<>();
        List<Rect> expectedRects = new ArrayList<>();
        List<Boolean> expectedVisibility = new ArrayList<>();
        expectedViews.add(subFrame1.first);
        expectedViews.add(subFrame2.first);
        expectedRects.add(subFrame1.second);
        expectedRects.add(subFrame2.second);
        expectedVisibility.add(true);
        expectedVisibility.add(true);
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));

        expectedRects.clear();
        expectedRects.add(new Rect(20, 40, 120, 80));
        expectedRects.add(new Rect(0, 0, 0, 0));
        expectedVisibility.set(1, false);

        // During scaling the second subframe should disappear from the viewport.
        Assert.assertTrue(mScaleController.scaleBy(2f, 0f, 0f));
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));
        Matrix expectedMatrix = new Matrix();
        expectedMatrix.setScale(2f, 2f);
        inOrderMediator1.verify(subFrame1Mediator)
                .setBitmapScaleMatrixOfSubframe(argThat(new MatrixMatcher(expectedMatrix)), eq(2f));

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0f, 0f));
        mBitmapStateController.swapForTest();
        inOrderMediator1.verify(subFrame1Mediator).updateScaleFactor(eq(2f));
        inOrderMediator1.verify(subFrame1Mediator).forceRedraw();
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));
        expectedMatrix.reset();
        inOrderMediator1.verify(subFrame1Mediator)
                .setBitmapScaleMatrixOfSubframe(argThat(new MatrixMatcher(expectedMatrix)), eq(1f));

        // Scroll so the second subframe is back in the viewport..
        mScrollController.scrollBy(20, 40);
        expectedRects.clear();
        expectedRects.add(new Rect(0, 0, 100, 40));
        expectedRects.add(new Rect(40, 60, 120, 280));
        expectedVisibility.clear();
        expectedVisibility.add(true);
        expectedVisibility.add(true);
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));

        // Scale out keeping the subframes in the viewport..
        Assert.assertTrue(mScaleController.scaleBy(0.75f, 25f, 50f));
        expectedRects.clear();
        expectedRects.add(new Rect(6, 12, 81, 42));
        expectedRects.add(new Rect(36, 57, 96, 222));
        Assert.assertEquals(expectedViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        Assert.assertEquals(expectedVisibility,
                getVisibilities(mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS)));
        expectedMatrix.setScale(0.75f, 0.75f);
        inOrderMediator1.verify(subFrame1Mediator)
                .setBitmapScaleMatrixOfSubframe(
                        argThat(new MatrixMatcher(expectedMatrix)), eq(1.5f));
        inOrderMediator2.verify(subFrame2Mediator)
                .setBitmapScaleMatrixOfSubframe(
                        argThat(new MatrixMatcher(expectedMatrix)), eq(1.5f));
    }

    /**
     * Tests that {@link PlayerFrameMediator} works correctly with nested subframes. This test
     * pretends that mMediator is for a subframe. The calls made to this mediator are verified
     * to occur in {@link testViewPortOnScaleByWithSubFrames}.
     */
    @Test
    public void testViewPortOnScaleByWithNestedSubFrames() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        View subframeView = new View(context);

        PlayerFrameMediator subFrameMediator = Mockito.mock(PlayerFrameMediator.class);
        Pair<View, Rect> subFrame = new Pair<>(subframeView, new Rect(10, 20, 60, 40));
        mMediator.addSubFrame(subFrame.first, subFrame.second, subFrameMediator);
        InOrder inOrder = inOrder(subFrameMediator);

        // The subframe should be visible.
        mMediator.updateViewportSize(50, 100, 1f);
        List<View> expectedVisibleViews = new ArrayList<>();
        List<Rect> expectedVisibleRects = new ArrayList<>();
        expectedVisibleViews.add(subFrame.first);
        expectedVisibleRects.add(subFrame.second);
        Assert.assertEquals(expectedVisibleViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedVisibleRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));

        expectedVisibleViews.clear();
        expectedVisibleRects.clear();
        expectedVisibleViews.add(subFrame.first);
        expectedVisibleRects.add(new Rect(20, 40, 120, 80));

        // Scale by a factor of two via the parent.
        Matrix scaleMatrix = new Matrix();
        scaleMatrix.setScale(2f, 2f);
        mMediator.setBitmapScaleMatrixOfSubframe(scaleMatrix, 2f);
        Assert.assertEquals(expectedVisibleViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedVisibleRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        inOrder.verify(subFrameMediator)
                .setBitmapScaleMatrixOfSubframe(argThat(new MatrixMatcher(scaleMatrix)), eq(2f));

        expectedVisibleViews.clear();
        expectedVisibleRects.clear();
        expectedVisibleViews.add(subFrame.first);
        expectedVisibleRects.add(new Rect(15, 30, 90, 60));

        // Zoom out by a factor of 0.75, note the scale factor argument is compounded.
        scaleMatrix.setScale(0.75f, 0.75f);
        mMediator.setBitmapScaleMatrixOfSubframe(scaleMatrix, 1.5f);
        Assert.assertEquals(expectedVisibleViews, mModel.get(PlayerFrameProperties.SUBFRAME_VIEWS));
        Assert.assertEquals(expectedVisibleRects, mModel.get(PlayerFrameProperties.SUBFRAME_RECTS));
        inOrder.verify(subFrameMediator)
                .setBitmapScaleMatrixOfSubframe(argThat(new MatrixMatcher(scaleMatrix)), eq(1.5f));

        // Simulate scaleFinished() by force a scale factor clear and redraw.
        mMediator.updateScaleFactor(1.5f);
        inOrder.verify(subFrameMediator).updateScaleFactor(eq(1.5f));
        mMediator.forceRedraw();
        inOrder.verify(subFrameMediator).forceRedraw();
    }

    /**
     * Tests that {@link PlayerFrameMediator} calls the user interaction callback.
     */
    @Test
    public void testUserInteractionCallback() {
        mMediator.updateViewportSize(100, 200, 1f);

        Assert.assertFalse(
                "User interaction callback shouldn't have been called", mHasUserInteraction);
        mScrollController.scrollBy(0, 10);
        Assert.assertTrue("User interaction callback should have been called", mHasUserInteraction);

        mHasUserInteraction = false;
        mScrollController.onFling(0, 10);
        Assert.assertTrue("User interaction callback should have been called", mHasUserInteraction);

        mHasUserInteraction = false;
        mScaleController.scaleBy(1.5f, 20, 30);
        Assert.assertTrue("User interaction callback should have been called", mHasUserInteraction);
    }

    /**
     * Tests that bitmap matrix is offset correctly.
     */
    @Test
    public void testOffsetBitmapScaleMatrix() {
        Matrix bitmapScaleMatrix = mModel.get(PlayerFrameProperties.SCALE_MATRIX);
        Matrix expectedBitmapScaleMatrix = new Matrix();
        mMediator.offsetBitmapScaleMatrix(5f, 10f); // Should no-op.
        Assert.assertEquals(expectedBitmapScaleMatrix, bitmapScaleMatrix);

        bitmapScaleMatrix.postScale(2f, 2f);
        mMediator.offsetBitmapScaleMatrix(5f, 10f);
        expectedBitmapScaleMatrix.postScale(2f, 2f);
        expectedBitmapScaleMatrix.postTranslate(-5f, -10f);
        Assert.assertEquals(expectedBitmapScaleMatrix, bitmapScaleMatrix);
    }
}
