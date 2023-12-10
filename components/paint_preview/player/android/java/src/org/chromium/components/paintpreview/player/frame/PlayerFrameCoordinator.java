// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;
import android.view.ViewConfiguration;
import android.widget.OverScroller;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UnguessableToken;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.paintpreview.player.OverscrollHandler;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.components.paintpreview.player.PlayerGestureListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Sets up the view and the logic behind it for a Paint Preview frame. */
public class PlayerFrameCoordinator {
    private PlayerFrameMediator mMediator;
    private PlayerFrameScaleController mScaleController;
    private PlayerFrameScrollController mScrollController;
    private PlayerFrameView mView;
    private List<PlayerFrameCoordinator> mSubFrames = new ArrayList<>();

    /**
     * Creates a {@link PlayerFrameMediator} and {@link PlayerFrameView} for this component and
     * binds them together.
     */
    public PlayerFrameCoordinator(
            Context context,
            PlayerCompositorDelegate compositorDelegate,
            UnguessableToken frameGuid,
            int contentWidth,
            int contentHeight,
            int initialScrollX,
            int initialScrollY,
            float initialScaleFactor,
            boolean canDetectZoom,
            @Nullable OverscrollHandler overscrollHandler,
            PlayerGestureListener gestureHandler,
            @Nullable Runnable firstPaintListener,
            @Nullable Supplier<Boolean> isAccessibilityEnabled,
            @Nullable Runnable initialViewportSizeAvailable) {
        PropertyModel model = new PropertyModel.Builder(PlayerFrameProperties.ALL_KEYS).build();
        OverScroller scroller = new OverScroller(context);
        scroller.setFriction(ViewConfiguration.getScrollFriction() / 2);

        mMediator =
                new PlayerFrameMediator(
                        model,
                        compositorDelegate,
                        gestureHandler,
                        frameGuid,
                        new Size(contentWidth, contentHeight),
                        initialScrollX,
                        initialScrollY,
                        initialScaleFactor,
                        initialViewportSizeAvailable);

        if (canDetectZoom) {
            mScaleController =
                    new PlayerFrameScaleController(
                            model.get(PlayerFrameProperties.SCALE_MATRIX),
                            mMediator,
                            isAccessibilityEnabled,
                            gestureHandler::onScale);
        }
        mScrollController =
                new PlayerFrameScrollController(
                        scroller, mMediator, gestureHandler::onScroll, gestureHandler::onFling);
        PlayerFrameGestureDetectorDelegate gestureDelegate =
                new PlayerFrameGestureDetectorDelegate(
                        mScaleController, mScrollController, mMediator);

        mView =
                new PlayerFrameView(
                        context, canDetectZoom, mMediator, gestureDelegate, firstPaintListener);

        if (overscrollHandler != null) {
            mScrollController.setOverscrollHandler(overscrollHandler);
        }
        PropertyModelChangeProcessor.create(model, mView, PlayerFrameViewBinder::bind);
    }

    public void destroy() {
        // Destroy the view first to unlock all bitmaps so they can be destroyed successfully.
        mView.destroy();
        mMediator.destroy();
        for (PlayerFrameCoordinator subframe : mSubFrames) {
            subframe.destroy();
        }
    }

    public void setAcceptUserInput(boolean acceptUserInput) {
        if (mScrollController != null) mScrollController.setAcceptUserInput(acceptUserInput);
        if (mScaleController != null) mScaleController.setAcceptUserInput(acceptUserInput);
        for (PlayerFrameCoordinator subFrame : mSubFrames) {
            subFrame.setAcceptUserInput(acceptUserInput);
        }
    }

    public Point getScrollPosition() {
        Rect viewPortRect = mMediator.getViewport().asRect();
        return new Point(viewPortRect.left, viewPortRect.top);
    }

    public float getScale() {
        return mMediator.getViewport().getScale();
    }

    /**
     * Adds a child {@link PlayerFrameCoordinator} to this class.
     * @param subFrame The sub-frame's {@link PlayerFrameCoordinator}.
     * @param clipRect The {@link Rect} in which this sub-frame should be shown in.
     */
    public void addSubFrame(PlayerFrameCoordinator subFrame, Rect clipRect) {
        mSubFrames.add(subFrame);
        mMediator.addSubFrame(subFrame.mView, clipRect, subFrame.getMediator());
        subFrame.mView.getGestureDetector().setParentGestureDetector(mView.getGestureDetector());
    }

    public PlayerFrameViewport getViewportForAccessibility() {
        if (mMediator == null) return null;

        return mMediator.getViewport();
    }

    public PlayerFrameCoordinator getSubFrameForAccessibility(int index) {
        if (index > mSubFrames.size()) return null;

        return mSubFrames.get(index);
    }

    public Size getContentSizeForAccessibility() {
        return mMediator.getContentSize();
    }

    public void handleClickForAccessibility(int x, int y, boolean isAbsolute) {
        mMediator.onTap(x, y, isAbsolute);
    }

    public void scrollToMakeRectVisibleForAccessibility(Rect rect) {
        mScrollController.scrollToMakeRectVisibleForAccessibility(rect);
    }

    public void setOnScrollCallbackForAccessibility(Runnable onScrollCallback) {
        mScrollController.setOnScrollCallbackForAccessibility(onScrollCallback);
    }

    /** @return The mediator associated with this component. */
    @VisibleForTesting
    PlayerFrameMediator getMediator() {
        return mMediator;
    }

    /** @return The view associated with this component. */
    public PlayerFrameView getView() {
        return mView;
    }

    public boolean checkRequiredBitmapsLoadedForTest() {
        return mMediator.checkRequiredBitmapsLoadedForTest();
    }

    PlayerFrameScaleController getScaleControllerForTest() {
        return mScaleController;
    }

    PlayerFrameScrollController getScrollControllerForTest() {
        return mScrollController;
    }
}
