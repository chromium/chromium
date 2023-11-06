// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Matrix;
import android.util.Size;
import android.widget.OverScroller;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.paintpreview.player.OverscrollHandler;

/** Tests for the {@link PlayerFrameScrollController} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {PaintPreviewCustomFlingingShadowScroller.class})
public class PlayerFrameScrollControllerTest {
    private static final int CONTENT_WIDTH = 500;
    private static final int CONTENT_HEIGHT = 1000;
    private static final float TOLERANCE = 0.001f;

    private OverScroller mScroller;
    private PlayerFrameViewport mViewport;
    @Mock private PlayerFrameMediatorDelegate mMediatorDelegateMock;
    @Mock private OverscrollHandler mOverscrollHandlerMock;
    private boolean mDidScroll;
    private boolean mDidFling;
    private PlayerFrameScrollController mScrollController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mScroller = new OverScroller(ContextUtils.getApplicationContext());
        mDidScroll = false;
        Runnable mOnScrollListener = () -> mDidScroll = true;
        Runnable mOnFlingListener = () -> mDidFling = true;
        mViewport = new PlayerFrameViewport();
        when(mMediatorDelegateMock.getViewport()).thenReturn(mViewport);
        when(mMediatorDelegateMock.getContentSize())
                .thenReturn(new Size(CONTENT_WIDTH, CONTENT_HEIGHT));
        mScrollController =
                new PlayerFrameScrollController(
                        mScroller, mMediatorDelegateMock, mOnScrollListener, mOnFlingListener);
    }

    /** Test that scrolling updates the viewport correctly and triggers expected callbacks. */
    @Test
    public void testScrollBy() {
        mViewport.setSize(100, 100);

        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);

        Assert.assertTrue(mScrollController.scrollBy(100, 100));
        Assert.assertEquals(100f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(100f, mViewport.getTransY(), TOLERANCE);
        verify(mMediatorDelegateMock).updateVisuals(eq(false));
        Assert.assertTrue(mDidScroll);
    }

    /** Test that scrolling won't exceed content bounds. */
    @Test
    public void testScrollByWithinBounds() {
        mViewport.setSize(100, 100);

        // Attempt to scroll out-of-bounds left.
        Assert.assertFalse(mScrollController.scrollBy(-100, 0));
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);

        // Attempt to scroll out-of-bounds up.
        Assert.assertFalse(mScrollController.scrollBy(0, -100));
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);

        // Overscroll downwards.
        Assert.assertTrue(mScrollController.scrollBy(0, 2000));
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(900f, mViewport.getTransY(), TOLERANCE);

        // Overscroll right.
        Assert.assertTrue(mScrollController.scrollBy(1000, 0));
        Assert.assertEquals(400f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(900f, mViewport.getTransY(), TOLERANCE);

        // Attempt to scroll out-of-bounds right.
        Assert.assertFalse(mScrollController.scrollBy(100, 0));
        Assert.assertEquals(400f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(900f, mViewport.getTransY(), TOLERANCE);

        // Attempt to scroll out-of-bounds down.
        Assert.assertFalse(mScrollController.scrollBy(0, 100));
        Assert.assertEquals(400f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(900f, mViewport.getTransY(), TOLERANCE);

        // Overscroll upward.
        Assert.assertTrue(mScrollController.scrollBy(0, -2000));
        Assert.assertEquals(400f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);

        // Overscroll left.
        Assert.assertTrue(mScrollController.scrollBy(-1000, 0));
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);
    }

    /**
     * Test that flinging updates the viewport correctly and triggers expected callbacks. NOTE: this
     * uses a custom shadow for the OverScroller that immediately scrolls to top or bottom of the
     * page in one step.
     */
    @Test
    public void testOnFling() {
        mViewport.setSize(100, 100);

        Assert.assertTrue(mScrollController.onFling(100, 0));
        Assert.assertTrue(mDidFling);
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(mScroller.getFinalX(), mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(mScroller.getFinalY(), mViewport.getTransY(), TOLERANCE);

        Assert.assertTrue(mScrollController.onFling(-100, 0));
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(mScroller.getFinalX(), mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(mScroller.getFinalY(), mViewport.getTransY(), TOLERANCE);

        Assert.assertTrue(mScrollController.onFling(0, 100));
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(mScroller.getFinalX(), mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(mScroller.getFinalY(), mViewport.getTransY(), TOLERANCE);

        Assert.assertTrue(mScrollController.onFling(0, -100));
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(mScroller.getFinalX(), mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(mScroller.getFinalY(), mViewport.getTransY(), TOLERANCE);

        Assert.assertTrue(mScrollController.onFling(100, 100));
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(mScroller.getFinalX(), mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(mScroller.getFinalY(), mViewport.getTransY(), TOLERANCE);

        Assert.assertTrue(mScrollController.onFling(-100, -100));
        ShadowLooper.runUiThreadTasks();
        Assert.assertTrue(mScroller.isFinished());
        Assert.assertEquals(mScroller.getFinalX(), mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(mScroller.getFinalY(), mViewport.getTransY(), TOLERANCE);
    }

    /** Test that the overscroll-to-refresh handler is called when appropriate. */
    @Test
    public void testOverscrollToRefresh() {
        mScrollController.setOverscrollHandler(mOverscrollHandlerMock);
        mViewport.setSize(100, 100);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);

        when(mOverscrollHandlerMock.start()).thenReturn(true);
        Assert.assertTrue(mScrollController.scrollBy(0, -100));
        verify(mOverscrollHandlerMock).start();
        verify(mOverscrollHandlerMock).pull(eq(100f));

        mScrollController.onRelease();
        verify(mOverscrollHandlerMock).release();
    }

    /** Test that the overscroll-to-refresh handler is eased correctly. */
    @Test
    public void testOverscrollToRefreshEasedOff() {
        mScrollController.setOverscrollHandler(mOverscrollHandlerMock);
        mViewport.setSize(100, 100);
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(0f, mViewport.getTransY(), TOLERANCE);

        when(mOverscrollHandlerMock.start()).thenReturn(true);
        Assert.assertTrue(mScrollController.scrollBy(0, -100));
        verify(mOverscrollHandlerMock).start();
        verify(mOverscrollHandlerMock).pull(eq(100f));

        Assert.assertTrue(mScrollController.scrollBy(0, 100));
        verify(mOverscrollHandlerMock).reset();

        mScrollController.onRelease();
        verify(mOverscrollHandlerMock, never()).release();
    }

    /** Test that the bitmap scale matrix updates correctly if it isn't identity. */
    @Test
    public void testOffsetBitmapScaleMatrix() {
        mViewport.setSize(100, 100);
        mViewport.setScale(2f);
        InOrder inOrder = inOrder(mMediatorDelegateMock);
        Matrix expectedMatrix = new Matrix();

        // Overscroll downwards.
        expectedMatrix.postScale(2f, 2f);
        expectedMatrix.postTranslate(0, -1900);
        Assert.assertTrue(mScrollController.scrollBy(0, 2000));
        Assert.assertEquals(0f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(1900f, mViewport.getTransY(), TOLERANCE);
        inOrder.verify(mMediatorDelegateMock).offsetBitmapScaleMatrix(eq(0f), eq(1900f));

        // Overscroll right.
        expectedMatrix.postTranslate(-900, 0);
        Assert.assertTrue(mScrollController.scrollBy(1000, 0));
        Assert.assertEquals(900f, mViewport.getTransX(), TOLERANCE);
        Assert.assertEquals(1900f, mViewport.getTransY(), TOLERANCE);
        inOrder.verify(mMediatorDelegateMock).offsetBitmapScaleMatrix(eq(900f), eq(0f));
    }
}
