// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;

import android.graphics.Matrix;
import android.util.Size;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the {@link PlayerFrameScaleController} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerFrameScaleControllerTest {
    private static final int CONTENT_WIDTH = 500;
    private static final int CONTENT_HEIGHT = 1000;
    private static final float TOLERANCE = 0.001f;

    private Matrix mBitmapScaleMatrix;
    private PlayerFrameViewport mViewport;
    private PlayerFrameScaleController mScaleController;
    @Mock private PlayerFrameMediatorDelegate mMediatorDelegateMock;
    private boolean mDidScale;

    private static class MatrixMatcher implements ArgumentMatcher<Matrix> {
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
        MockitoAnnotations.initMocks(this);
        mDidScale = false;
        Callback<Boolean> mScaleListener = (Boolean didFinish) -> mDidScale = true;
        mViewport = new PlayerFrameViewport();
        mBitmapScaleMatrix = new Matrix();
        Size contentSize = new Size(CONTENT_WIDTH, CONTENT_HEIGHT);
        when(mMediatorDelegateMock.getViewport()).thenReturn(mViewport);
        when(mMediatorDelegateMock.getContentSize()).thenReturn(contentSize);
        when(mMediatorDelegateMock.getMinScaleFactor()).thenReturn(1f);
        mScaleController =
                new PlayerFrameScaleController(
                        mBitmapScaleMatrix, mMediatorDelegateMock, null, mScaleListener);
        mViewport.setScale(1f);
        mViewport.setSize(100, 100);
    }

    /** Tests the limits of scaling. */
    @Test
    public void testScaleLimits() {
        Assert.assertTrue(mScaleController.scaleBy(10f, 0, 0));
        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(5f, mViewport.getScale(), TOLERANCE);

        Assert.assertTrue(mScaleController.scaleBy(0.00001f, 0, 0));
        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(1f, mViewport.getScale(), TOLERANCE);
    }

    /** Scales the viewport in and out in the middle so no correction occurs. */
    @Test
    public void testZoomInAndOutAtMiddle() {
        mViewport.setTrans(100, 150);
        InOrder inOrder = inOrder(mMediatorDelegateMock);

        // Zoom in.
        Assert.assertTrue(mScaleController.scaleBy(2f, 50, 50));
        Matrix expectedBitmapMatrix = new Matrix();
        expectedBitmapMatrix.postScale(2f, 2f, 50, 50);
        Assert.assertEquals(2f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(250f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(350f, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(2f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(2f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(250f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(350f, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(eq(2f));
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();

        // Pretend images were fetched and bitmap scale is reset.
        mBitmapScaleMatrix.reset();

        // Zoom out.
        Assert.assertTrue(mScaleController.scaleBy(0.5f, 50, 50));
        expectedBitmapMatrix.postScale(0.5f, 0.5f, 50, 50);
        Assert.assertEquals(1f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(100f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(150f, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(1f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(1f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(100f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(150f, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(eq(1f));
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();
    }

    /** Scales the viewport in and out in the top left so correction occurs. */
    @Test
    public void testZoomInAndOutAtTopLeft() {
        InOrder inOrder = inOrder(mMediatorDelegateMock);

        // Zoom in.
        Assert.assertTrue(mScaleController.scaleBy(2f, 0, 0));
        Matrix expectedBitmapMatrix = new Matrix();
        expectedBitmapMatrix.postScale(2f, 2f, 0, 0);
        Assert.assertEquals(2f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(2f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(2f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(eq(2f));
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();

        // Pretend images were fetched and bitmap scale is reset.
        mBitmapScaleMatrix.reset();

        // Zoom out.
        Assert.assertTrue(mScaleController.scaleBy(0.75f, 50, 50));
        expectedBitmapMatrix.postScale(0.75f, 0.75f, 0f, 0f);
        // Positional compensation due to smaller bitmaps.
        Assert.assertEquals(1.5f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(1.5f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(1.5f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(eq(1.5f));
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();
    }

    /** Scales the viewport in and out in the bottom right so correction occurs. */
    @Test
    public void testZoomInAndOutAtBottomRight() {
        InOrder inOrder = inOrder(mMediatorDelegateMock);

        // Zoom in.
        Assert.assertTrue(mScaleController.scaleBy(1.5f, 0, 0));
        Matrix expectedBitmapMatrix = new Matrix();
        expectedBitmapMatrix.postScale(1.5f, 1.5f, 0, 0);
        Assert.assertEquals(1.5f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(1.5f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(1.5f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(eq(1.5f));
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();

        // Pretend images were fetched and bitmap scale is reset.
        mBitmapScaleMatrix.reset();

        // Move to the bottom right corner.
        float scale = mViewport.getScale();
        float scaledContentWidth = scale * CONTENT_WIDTH;
        float scaledContentHeight = scale * CONTENT_HEIGHT;
        mViewport.setTrans(
                scaledContentWidth - mViewport.getWidth(),
                scaledContentHeight - mViewport.getHeight());

        // Zoom out.
        Assert.assertTrue(mScaleController.scaleBy(0.75f, -50, -50));
        expectedBitmapMatrix.postScale(0.75f, 0.75f, 0f, 0f);
        // Positional compensation due to smaller bitmaps.
        expectedBitmapMatrix.postTranslate(25f, 25f);
        Assert.assertEquals(1.125f, mViewport.getScale(), TOLERANCE);
        scale = mViewport.getScale();
        scaledContentWidth = scale * CONTENT_WIDTH;
        scaledContentHeight = scale * CONTENT_HEIGHT;
        final float expectedX = scaledContentWidth - mViewport.getWidth();
        final float expectedY = scaledContentHeight - mViewport.getHeight();
        Assert.assertEquals(expectedX, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(expectedY, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(1.125f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(1.125f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(expectedX, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(expectedY, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(eq(1.125f));
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();
    }

    /** Scales the viewport without a reset of the bitmap scale matrix. */
    @Test
    public void testZoomInAndOutWithoutReset() {
        mViewport.setTrans(100, 150);
        InOrder inOrder = inOrder(mMediatorDelegateMock);

        // Zoom in.
        Assert.assertTrue(mScaleController.scaleBy(2f, 50, 50));
        Matrix expectedBitmapMatrix = new Matrix();
        expectedBitmapMatrix.postScale(2f, 2f, 50, 50);
        Assert.assertEquals(2f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(250f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(350f, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(2f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(2f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(250f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(350f, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(2f);
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();

        // Pretend images weren't fetched and bitmap scale is not reset.

        // Zoom out.
        Assert.assertTrue(mScaleController.scaleBy(0.75f, 50, 50));
        expectedBitmapMatrix.reset();
        expectedBitmapMatrix.postScale(1.5f, 1.5f);
        expectedBitmapMatrix.postTranslate(-25, -25);
        Assert.assertEquals(1.5f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(175f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(250f, mViewport.getTransY(), TOLERANCE);
        Assert.assertEquals(expectedBitmapMatrix, mBitmapScaleMatrix);
        inOrder.verify(mMediatorDelegateMock)
                .setBitmapScaleMatrix(argThat(new MatrixMatcher(expectedBitmapMatrix)), eq(1.5f));
        Assert.assertTrue(mDidScale);

        Assert.assertTrue(mScaleController.scaleFinished(1f, 0, 0));
        Assert.assertEquals(1.5f, mViewport.getScale(), TOLERANCE);
        Assert.assertEquals(175f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(250f, mViewport.getTransY(), TOLERANCE);
        expectedBitmapMatrix.reset();
        inOrder.verify(mMediatorDelegateMock).updateScaleFactorOfAllSubframes(1.5f);
        inOrder.verify(mMediatorDelegateMock).updateVisuals(eq(true));
        inOrder.verify(mMediatorDelegateMock).forceRedrawVisibleSubframes();
    }
}
