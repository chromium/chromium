// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.Size;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeProvider;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.List;

/**
 * Responsible for detecting touch gestures, displaying the content of a frame and its sub-frames.
 * {@link PlayerFrameBitmapPainter} is used for drawing the contents. Sub-frames are represented
 * with individual {@link View}s. {@link #mSubFrames} contains the list of all sub-frames and their
 * relative positions.
 */
public class PlayerFrameView extends FrameLayout {
    private PlayerFrameBitmapPainter mBitmapPainter;
    private PlayerFrameGestureDetector mGestureDetector;
    private PlayerFrameViewDelegate mDelegate;
    private List<View> mSubFrameViews;
    private List<Rect> mSubFrameRects;
    private Matrix mScaleMatrix;
    private Matrix mOffset = new Matrix();
    protected WebContentsAccessibility mWebContentsAccessibility;

    /**
     * @param context                 Used for initialization.
     * @param canDetectZoom           Whether this {@link View} should detect zoom (scale)
     *                                gestures.
     * @param playerFrameViewDelegate The interface used for forwarding events.
     */
    public PlayerFrameView(
            @NonNull Context context,
            boolean canDetectZoom,
            PlayerFrameViewDelegate playerFrameViewDelegate,
            PlayerFrameGestureDetectorDelegate gestureDetectorDelegate,
            @Nullable Runnable firstPaintListener) {
        super(context);
        setWillNotDraw(false);
        mDelegate = playerFrameViewDelegate;
        mBitmapPainter = new PlayerFrameBitmapPainter(this::postInvalidate, firstPaintListener);
        mGestureDetector =
                new PlayerFrameGestureDetector(context, canDetectZoom, gestureDetectorDelegate);
    }

    /** Sets the {@link WebContentsAccessibility} for this View. */
    public void setWebContentsAccessibility(WebContentsAccessibility webContentsAccessibility) {
        mWebContentsAccessibility = webContentsAccessibility;
    }

    PlayerFrameGestureDetector getGestureDetector() {
        return mGestureDetector;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        mDelegate.setLayoutDimensions(getWidth(), getHeight());
    }

    /**
     * Updates the sub-frame views that this {@link PlayerFrameView} should display.
     *
     * @param subFrameViews List of all sub-frame views.
     */
    void updateSubFrameViews(List<View> subFrameViews) {
        mSubFrameViews = subFrameViews;
    }

    /**
     * Updates clip rects for sub-frames that this {@link PlayerFrameView} should display.
     *
     * @param subFrameRects List of all sub-frames clip rects.
     */
    void updateSubFrameRects(List<Rect> subFrameRects) {
        mSubFrameRects = subFrameRects;
    }

    void updateOffset(int left, int top) {
        mOffset.setTranslate(left, top);
    }

    void updateViewPort(int left, int top, int right, int bottom) {
        mBitmapPainter.updateViewPort(left, top, right, bottom);
        layoutSubFrames();
    }

    void updateBitmapMatrix(Bitmap[][] bitmapMatrix) {
        mBitmapPainter.updateBitmapMatrix(bitmapMatrix);
    }

    void updateTileDimensions(Size tileDimensions) {
        mBitmapPainter.updateTileDimensions(tileDimensions);
    }

    void updateScaleMatrix(Matrix matrix) {
        mScaleMatrix = matrix;
        if (mScaleMatrix.isIdentity()) return;

        postInvalidate();
        layoutSubFrames();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.save();
        canvas.concat(mOffset);
        canvas.concat(mScaleMatrix);
        mBitmapPainter.onDraw(canvas);
        canvas.restore();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        return mGestureDetector.onTouchEvent(event);
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        if (mWebContentsAccessibility != null && AccessibilityState.isTouchExplorationEnabled()) {
            return mWebContentsAccessibility.onHoverEventNoRenderer(event);
        }
        return super.onHoverEvent(event);
    }

    private void layoutSubFrames() {
        // Remove all views if there are no sub-frames.
        if (mSubFrameViews == null || mSubFrameRects == null) {
            removeAllViews();
            return;
        }

        // Layout the sub-frames.
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            View subFrameView = mSubFrameViews.get(i);
            if (subFrameView.getVisibility() != View.VISIBLE) {
                removeView(subFrameView);
                continue;
            }

            if (subFrameView.getParent() == null) {
                addView(mSubFrameViews.get(i));
            } else if (subFrameView.getParent() != this) {
                throw new IllegalStateException("Sub-frame view already has a parent.");
            }
            Rect layoutRect = mSubFrameRects.get(i);
            subFrameView.layout(
                    layoutRect.left, layoutRect.top, layoutRect.right, layoutRect.bottom);
        }
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        AccessibilityNodeProvider provider =
                (mWebContentsAccessibility != null)
                        ? mWebContentsAccessibility.getAccessibilityNodeProvider()
                        : null;
        return (provider != null) ? provider : super.getAccessibilityNodeProvider();
    }

    @Override
    public void onProvideVirtualStructure(final ViewStructure structure) {
        if (mWebContentsAccessibility != null) {
            mWebContentsAccessibility.onProvideVirtualStructure(structure, false);
        }
    }

    void destroy() {
        mBitmapPainter.destroy();
    }
}
