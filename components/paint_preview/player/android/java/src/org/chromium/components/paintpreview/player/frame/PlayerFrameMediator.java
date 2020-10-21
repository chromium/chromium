// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.UnguessableToken;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.components.paintpreview.player.PlayerGestureListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles the business logic for the player frame component. Concretely, this class is responsible
 * for:
 * <ul>
 * <li>Maintaining a viewport {@link Rect} that represents the current user-visible section of this
 * frame. The dimension of the viewport is constant and is equal to the initial values received on
 * {@link #setLayoutDimensions}.</li>
 * <li>Constructing a matrix of {@link CompressibleBitmap} tiles that represents the content of this
 * frame for a given scale factor. Each tile is as big as the view port.</li>
 * <li>Requesting bitmaps from Paint Preview compositor.</li>
 * <li>Updating the viewport on touch gesture notifications (scrolling and scaling).<li/>
 * <li>Determining which sub-frames are visible given the current viewport and showing them.<li/>
 * </ul>
 */
class PlayerFrameMediator implements PlayerFrameViewDelegate, PlayerFrameMediatorDelegate {
    /** The GUID associated with the frame that this class is representing. */
    private final UnguessableToken mGuid;
    /** The size of the content inside this frame, at a scale factor of 1. */
    private final Size mContentSize;
    /**
     * Contains all {@link View}s corresponding to this frame's sub-frames.
     */
    private final List<View> mSubFrameViews = new ArrayList<>();
    /**
     * Contains all clip rects corresponding to this frame's sub-frames.
     */
    private final List<Rect> mSubFrameRects = new ArrayList<>();
    /**
     * Contains all mediators corresponding to this frame's sub-frames.
     */
    private final List<PlayerFrameMediator> mSubFrameMediators = new ArrayList<>();
    /**
     * Contains scaled clip rects corresponding to this frame's sub-frames.
     */
    private final List<Rect> mSubFrameScaledRects = new ArrayList<>();

    private final PropertyModel mModel;
    private final PlayerCompositorDelegate mCompositorDelegate;
    /** The viewport of this frame. */
    private final PlayerFrameViewport mViewport;

    private boolean mIsSubframe;
    private float mInitialScaleFactor;
    /** Handles scaling of bitmaps. */
    private final Matrix mBitmapScaleMatrix;

    private final PlayerFrameBitmapStateController mBitmapStateController;

    private PlayerGestureListener mGestureListener;

    PlayerFrameMediator(PropertyModel model, PlayerCompositorDelegate compositorDelegate,
            PlayerGestureListener gestureListener, UnguessableToken frameGuid, Size contentSize,
            int initialScrollX, int initialScrollY) {
        mBitmapScaleMatrix = new Matrix();
        mModel = model;
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);

        mCompositorDelegate = compositorDelegate;
        mGestureListener = gestureListener;
        mViewport = new PlayerFrameViewport();
        mIsSubframe = false;
        mInitialScaleFactor = 0f;
        mGuid = frameGuid;
        mContentSize = contentSize;
        SequencedTaskRunner taskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);
        mBitmapStateController = new PlayerFrameBitmapStateController(
                mGuid, mViewport, mContentSize, mCompositorDelegate, this, taskRunner);
        mViewport.offset(initialScrollX, initialScrollY);
        mViewport.setScale(0f);
    }

    @VisibleForTesting
    PlayerFrameBitmapStateController getBitmapStateControllerForTest() {
        return mBitmapStateController;
    }

    void updateViewportSize(int width, int height, float scaleFactor) {
        if (width <= 0 || height <= 0) return;

        // Ensure the viewport is within the bounds of the content.
        final int left = Math.max(0,
                Math.min(Math.round(mViewport.getTransX()),
                        Math.round(mContentSize.getWidth() * scaleFactor) - width));
        final int top = Math.max(0,
                Math.min(Math.round(mViewport.getTransY()),
                        Math.round(mContentSize.getHeight() * scaleFactor) - height));
        mViewport.setTrans(left, top);
        mViewport.setSize(width, height);
        final float oldScaleFactor = mViewport.getScale();
        mViewport.setScale(scaleFactor);
        updateVisuals(oldScaleFactor != scaleFactor);
    }

    /**
     * Adds a new sub-frame to this frame.
     * @param subFrameView The {@link View} associated with the sub-frame.
     * @param clipRect     The bounds of the sub-frame, relative to this frame.
     * @param mediator     The mediator of the sub-frame.
     */
    void addSubFrame(View subFrameView, Rect clipRect, PlayerFrameMediator mediator) {
        mSubFrameViews.add(subFrameView);
        mSubFrameRects.add(clipRect);
        mSubFrameMediators.add(mediator);
        mSubFrameScaledRects.add(new Rect());
        mediator.markAsSubframe();
        mediator.setInitialScaleFactor(mInitialScaleFactor);
        mModel.set(PlayerFrameProperties.SUBFRAME_VIEWS, mSubFrameViews);
        mModel.set(PlayerFrameProperties.SUBFRAME_RECTS, mSubFrameScaledRects);
    }

    void setBitmapScaleMatrixOfSubframe(Matrix matrix, float scaleFactor) {
        // Don't update the subframes if the matrix is identity as it will be forcibly recalculated.
        if (!matrix.isIdentity()) {
            updateSubframes(mViewport.asRect(), scaleFactor);
        }
        setBitmapScaleMatrix(matrix, scaleFactor);
    }

    // PlayerFrameViewDelegate

    @Override
    public void setLayoutDimensions(int width, int height) {
        // Don't trigger a re-draw if we are actively scaling.
        if (!mBitmapScaleMatrix.isIdentity()) {
            mViewport.setSize(width, height);
            return;
        }

        // Set initial scale so that content width fits within the layout dimensions.
        if (!mIsSubframe) {
            adjustInitialScaleFactor(width);
        }

        final float scaleFactor = mViewport.getScale();
        updateViewportSize(
                width, height, (scaleFactor == 0f) ? getInitialScaleFactor() : scaleFactor);
    }

    @Override
    public void onTap(int x, int y) {
        // x and y are in the View's coordinate system (scaled). This needs to be adjusted to the
        // absolute coordinate system for hit testing.
        final float scaleFactor = mViewport.getScale();
        GURL url = mCompositorDelegate.onClick(mGuid,
                Math.round((float) (mViewport.getTransX() + x) / scaleFactor),
                Math.round((float) (mViewport.getTransY() + y) / scaleFactor));
        mGestureListener.onTap(url);
    }

    @Override
    public void onLongPress(int x, int y) {
        mGestureListener.onLongPress();
    }

    // PlayerFrameMediatorDelegate

    @Override
    public PlayerFrameViewport getViewport() {
        return mViewport;
    }

    @Override
    public Size getContentSize() {
        return mContentSize;
    }

    @Override
    public float getInitialScaleFactor() {
        return mInitialScaleFactor;
    }

    @Override
    public void onStartScaling() {
        mBitmapStateController.onStartScaling();
    }

    @Override
    public void onSwapState() {
        PlayerFrameBitmapState bitmapState = mBitmapStateController.getBitmapState(false);
        mBitmapScaleMatrix.reset();
        setBitmapScaleMatrix(mBitmapScaleMatrix, 1f);
        mModel.set(PlayerFrameProperties.TILE_DIMENSIONS, bitmapState.getTileDimensions());
        mModel.set(PlayerFrameProperties.VIEWPORT, mViewport.asRect());
        mModel.set(PlayerFrameProperties.BITMAP_MATRIX, bitmapState.getMatrix());
    }

    @Override
    public void offsetBitmapScaleMatrix(float dx, float dy) {
        // If we are still waiting on new bitmaps after a scale operation, the scroll should scroll
        // the bitmaps we currently have. In order to do so we apply an opposite transform to the
        // bitmaps that are shown on the screen.
        if (!mBitmapScaleMatrix.isIdentity()) {
            float[] bitmapScaleMatrixValues = new float[9];
            mBitmapScaleMatrix.getValues(bitmapScaleMatrixValues);
            bitmapScaleMatrixValues[Matrix.MTRANS_X] -= dx;
            bitmapScaleMatrixValues[Matrix.MTRANS_Y] -= dy;
            mBitmapScaleMatrix.setValues(bitmapScaleMatrixValues);
            setBitmapScaleMatrix(mBitmapScaleMatrix, mViewport.getScale());
        }
    }

    /**
     * Called when the viewport is moved or the scale factor is changed. Updates the viewport
     * and requests bitmap tiles for portion of the view port that don't have bitmap tiles.
     * @param scaleUpdated Whether the scale was updated.
     */
    @Override
    public void updateVisuals(boolean scaleUpdated) {
        final float scaleFactor = mViewport.getScale();
        PlayerFrameBitmapState activeLoadingState =
                mBitmapStateController.getBitmapState(scaleUpdated);

        // Scaling locks the visible state from further updates. If the state is locked we
        // should not progress updating anything other than |mBitmapScaleMatrix| until
        // a new state is present.
        if (activeLoadingState.isLocked()) return;

        Rect viewportRect = mViewport.asRect();
        updateSubframes(viewportRect, scaleFactor);
        // Let the view know |mViewport| changed. PropertyModelChangeProcessor is smart about
        // this and will only update the view if |mViewport|'s rect is actually changed.
        if (mBitmapStateController.isVisible(activeLoadingState)) {
            mModel.set(
                    PlayerFrameProperties.TILE_DIMENSIONS, activeLoadingState.getTileDimensions());
            mModel.set(PlayerFrameProperties.VIEWPORT, viewportRect);
        }

        // Clear the required bitmaps matrix. It will be updated in #requestBitmapForTile.
        activeLoadingState.clearRequiredBitmaps();

        // Request bitmaps for tiles inside the view port that don't already have a bitmap.
        activeLoadingState.requestBitmapForRect(viewportRect);
    }

    @Override
    public void updateScaleFactorOfAllSubframes(float scaleFactor) {
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).updateScaleFactor(scaleFactor);
        }
    }

    @Override
    public void forceRedrawVisibleSubframes() {
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).forceRedraw();
        }
    }

    @Override
    public void updateBitmapMatrix(CompressibleBitmap[][] bitmapMatrix) {
        mModel.set(PlayerFrameProperties.BITMAP_MATRIX, bitmapMatrix);
    }

    @Override
    public void updateSubframes(Rect viewport, float scaleFactor) {
        for (int i = 0; i < mSubFrameRects.size(); i++) {
            Rect subFrameScaledRect = mSubFrameScaledRects.get(i);
            scaleRect(mSubFrameRects.get(i), subFrameScaledRect, scaleFactor);
            if (!Rect.intersects(subFrameScaledRect, viewport)) {
                mSubFrameViews.get(i).setVisibility(View.GONE);
                subFrameScaledRect.set(0, 0, 0, 0);
                continue;
            }

            int transformedLeft = subFrameScaledRect.left - viewport.left;
            int transformedTop = subFrameScaledRect.top - viewport.top;
            subFrameScaledRect.set(transformedLeft, transformedTop,
                    transformedLeft + subFrameScaledRect.width(),
                    transformedTop + subFrameScaledRect.height());
            mSubFrameViews.get(i).setVisibility(View.VISIBLE);
        }
        mModel.set(PlayerFrameProperties.SUBFRAME_RECTS, mSubFrameScaledRects);
        mModel.set(PlayerFrameProperties.SUBFRAME_VIEWS, mSubFrameViews);
    }

    @Override
    public void setBitmapScaleMatrix(Matrix matrix, float scaleFactor) {
        float[] matrixValues = new float[9];
        matrix.getValues(matrixValues);
        mBitmapScaleMatrix.setValues(matrixValues);
        Matrix childBitmapScaleMatrix = new Matrix();
        childBitmapScaleMatrix.setScale(
                matrixValues[Matrix.MSCALE_X], matrixValues[Matrix.MSCALE_Y]);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).setBitmapScaleMatrixOfSubframe(
                    childBitmapScaleMatrix, scaleFactor);
        }
        mModel.set(PlayerFrameProperties.SCALE_MATRIX, mBitmapScaleMatrix);
    }

    // Internal methods

    private void setInitialScaleFactor(float scaleFactor) {
        mInitialScaleFactor = scaleFactor;

        if (mSubFrameViews == null) return;

        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).setInitialScaleFactor(mInitialScaleFactor);
        }
    }

    private void markAsSubframe() {
        mIsSubframe = true;
    }

    @VisibleForTesting
    void updateScaleFactor(float scaleFactor) {
        mViewport.setScale(scaleFactor);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).updateScaleFactor(scaleFactor);
        }
    }

    @VisibleForTesting
    void forceRedraw() {
        updateVisuals(true);
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            mSubFrameMediators.get(i).forceRedraw();
        }
    }

    private void scaleRect(Rect inRect, Rect outRect, float scaleFactor) {
        outRect.set((int) (((float) inRect.left) * scaleFactor),
                (int) (((float) inRect.top) * scaleFactor),
                (int) (((float) inRect.right) * scaleFactor),
                (int) (((float) inRect.bottom) * scaleFactor));
    }

    /**
     * Calculates the initial scale factor for a given viewport width.
     * @param width The viewport width.
     */
    private void adjustInitialScaleFactor(float width) {
        mInitialScaleFactor = width / ((float) mContentSize.getWidth());
        for (int i = 0; i < mSubFrameViews.size(); i++) {
            mSubFrameMediators.get(i).setInitialScaleFactor(mInitialScaleFactor);
        }
    }

    @VisibleForTesting
    public boolean checkRequiredBitmapsLoadedForTest() {
        PlayerFrameBitmapState state = mBitmapStateController.getBitmapState(false);
        assert mBitmapStateController.isVisible(state);
        boolean hasBitmaps = state.checkRequiredBitmapsLoadedForTest();
        if (!hasBitmaps) return false;

        for (int i = 0; i < mSubFrameViews.size(); i++) {
            if (mSubFrameViews.get(i).getVisibility() != View.VISIBLE) continue;

            hasBitmaps &= mSubFrameMediators.get(i).checkRequiredBitmapsLoadedForTest();
        }
        return hasBitmaps;
    }
}
