// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.components.paintpreview.player.frame.PlayerFrameCoordinator;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/**
 * This is the only public class in this package and is hence the access point of this component for
 * the outer world. Users should call {@link #destroy()}  to ensure the native part is destroyed.
 */
public class PlayerManager {
    /**
     * Users of the {@link PlayerManager} class have to implement and pass this interface in the
     * constructor.
     */
    public interface Listener {
        /**
         * Called when the compositor cannot be successfully initialized.
         */
        void onCompositorError(@CompositorStatus int status);

        /**
         * Called when the view is ready. Will not be called if onCompositorError is called prior to
         * the view being ready.
         */
        void onViewReady();

        /**
         * Called when the first paint event happens.
         */
        void onFirstPaint();

        /**
         * Called when the use interacts with the paint preview.
         */
        void onUserInteraction();

        /**
         * Called when a frustrated behavior is detected.
         */
        void onUserFrustration();

        /**
         * Called when the a pull to refresh gesture is performed.
         */
        void onPullToRefresh();

        /**
         * Called with a url to trigger a navigation.
         */
        void onLinkClick(GURL url);
    }

    private static PlayerCompositorDelegate.Factory sCompositorDelegateFactoryForTesting;

    private Context mContext;
    private PlayerCompositorDelegate mDelegate;
    private PlayerFrameCoordinator mRootFrameCoordinator;
    private FrameLayout mHostView;
    private static final String sInitEvent = "paint_preview PlayerManager init";
    private PlayerSwipeRefreshHandler mPlayerSwipeRefreshHandler;
    private PlayerGestureListener mPlayerGestureListener;
    private boolean mIgnoreInitialScrollOffset;
    private Listener mListener;

    /**
     * Creates a new {@link PlayerManager}.
     *
     * @param url                               The url for the stored content that should be
     *                                          shown.
     * @param context                           An instance of current Android {@link Context}.
     * @param nativePaintPreviewServiceProvider The native paint preview service.
     * @param directoryKey                      The key for the directory storing the data.
     * @param listener                          Interface that includes a number of callbacks.
     * @param ignoreInitialScrollOffset         If true the initial scroll state that is recorded at
     *                                          capture time is ignored.
     */
    public PlayerManager(GURL url, Context context,
            NativePaintPreviewServiceProvider nativePaintPreviewServiceProvider,
            String directoryKey, @NonNull Listener listener, int backgroundColor,
            boolean ignoreInitialScrollOffset) {
        TraceEvent.startAsync(sInitEvent, hashCode());
        mContext = context;
        mListener = listener;
        mDelegate = getCompositorDelegateFactory().create(nativePaintPreviewServiceProvider, url,
                directoryKey, this::onCompositorReady, mListener::onCompositorError);
        mHostView = new FrameLayout(mContext);
        mPlayerSwipeRefreshHandler =
                new PlayerSwipeRefreshHandler(mContext, mListener::onPullToRefresh);
        mPlayerGestureListener = new PlayerGestureListener(
                mListener::onLinkClick, mListener::onUserInteraction, mListener::onUserFrustration);
        mHostView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mHostView.setBackgroundColor(backgroundColor);
        mIgnoreInitialScrollOffset = ignoreInitialScrollOffset;
    }

    public void setAcceptUserInput(boolean acceptUserInput) {
        if (mRootFrameCoordinator == null) return;

        mRootFrameCoordinator.setAcceptUserInput(acceptUserInput);
    }

    /**
     * @return Current scroll position of the main frame. null if the player is not initialized.
     */
    public Point getScrollPosition() {
        if (mRootFrameCoordinator == null) return null;

        return mRootFrameCoordinator.getScrollPosition();
    }

    /**
     * Called by {@link PlayerCompositorDelegateImpl} when the compositor is initialized. This
     * method initializes a sub-component for each frame and adds the view for the root frame to
     * {@link #mHostView}.
     */
    private void onCompositorReady(UnguessableToken rootFrameGuid, UnguessableToken[] frameGuids,
            int[] frameContentSize, int[] scrollOffsets, int[] subFramesCount,
            UnguessableToken[] subFrameGuids, int[] subFrameClipRects) {
        PaintPreviewFrame rootFrame = buildFrameTreeHierarchy(rootFrameGuid, frameGuids,
                frameContentSize, scrollOffsets, subFramesCount, subFrameGuids, subFrameClipRects,
                mIgnoreInitialScrollOffset);

        mRootFrameCoordinator = new PlayerFrameCoordinator(mContext, mDelegate, rootFrame.getGuid(),
                rootFrame.getContentWidth(), rootFrame.getContentHeight(),
                rootFrame.getInitialScrollX(), rootFrame.getInitialScrollY(), true,
                mPlayerSwipeRefreshHandler, mPlayerGestureListener, mListener::onFirstPaint);
        buildSubFrameCoordinators(mRootFrameCoordinator, rootFrame);
        mHostView.addView(mRootFrameCoordinator.getView(),
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        if (mPlayerSwipeRefreshHandler != null) {
            mHostView.addView(mPlayerSwipeRefreshHandler.getView());
        }
        TraceEvent.finishAsync(sInitEvent, hashCode());
        mListener.onViewReady();
    }

    /**
     * This method builds a hierarchy of {@link PaintPreviewFrame}s from primitive variables that
     * originate from native. Detailed explanation of the parameters can be found in {@link
     * PlayerCompositorDelegateImpl#onCompositorReady}.
     *
     * @return The root {@link PaintPreviewFrame}
     */
    @VisibleForTesting
    static PaintPreviewFrame buildFrameTreeHierarchy(UnguessableToken rootFrameGuid,
            UnguessableToken[] frameGuids, int[] frameContentSize, int[] scrollOffsets,
            int[] subFramesCount, UnguessableToken[] subFrameGuids, int[] subFrameClipRects,
            boolean ignoreInitialScrollOffset) {
        Map<UnguessableToken, PaintPreviewFrame> framesMap = new HashMap<>();
        for (int i = 0; i < frameGuids.length; i++) {
            int initalScrollX = ignoreInitialScrollOffset ? 0 : scrollOffsets[i * 2];
            int initalScrollY = ignoreInitialScrollOffset ? 0 : scrollOffsets[(i * 2) + 1];
            framesMap.put(frameGuids[i],
                    new PaintPreviewFrame(frameGuids[i], frameContentSize[i * 2],
                            frameContentSize[(i * 2) + 1], initalScrollX, initalScrollY));
        }

        int subFrameIdIndex = 0;
        for (int i = 0; i < frameGuids.length; i++) {
            PaintPreviewFrame currentFrame = framesMap.get(frameGuids[i]);
            int currentFrameSubFrameCount = subFramesCount[i];
            PaintPreviewFrame[] subFrames = new PaintPreviewFrame[currentFrameSubFrameCount];
            Rect[] subFrameClips = new Rect[currentFrameSubFrameCount];
            for (int subFrameIndex = 0; subFrameIndex < currentFrameSubFrameCount;
                    subFrameIndex++, subFrameIdIndex++) {
                subFrames[subFrameIndex] = framesMap.get(subFrameGuids[subFrameIdIndex]);
                int x = subFrameClipRects[subFrameIdIndex * 4];
                int y = subFrameClipRects[subFrameIdIndex * 4 + 1];
                int width = subFrameClipRects[subFrameIdIndex * 4 + 2];
                int height = subFrameClipRects[subFrameIdIndex * 4 + 3];
                subFrameClips[subFrameIndex] = new Rect(x, y, x + width, y + height);
            }
            currentFrame.setSubFrames(subFrames);
            currentFrame.setSubFrameClips(subFrameClips);
        }
        return framesMap.get(rootFrameGuid);
    }

    /**
     * Recursively builds {@link PlayerFrameCoordinator}s for the sub-frames of the given frame and
     * adds them to the given frameCoordinator.
     */
    private void buildSubFrameCoordinators(
            PlayerFrameCoordinator frameCoordinator, PaintPreviewFrame frame) {
        if (frame.getSubFrames() == null || frame.getSubFrames().length == 0) {
            return;
        }

        for (int i = 0; i < frame.getSubFrames().length; i++) {
            PaintPreviewFrame childFrame = frame.getSubFrames()[i];
            PlayerFrameCoordinator childCoordinator = new PlayerFrameCoordinator(mContext,
                    mDelegate, childFrame.getGuid(), childFrame.getContentWidth(),
                    childFrame.getContentHeight(), childFrame.getInitialScrollX(),
                    childFrame.getInitialScrollY(), false, null, mPlayerGestureListener, null);
            buildSubFrameCoordinators(childCoordinator, childFrame);
            frameCoordinator.addSubFrame(childCoordinator, frame.getSubFrameClips()[i]);
        }
    }

    public void setCompressOnClose(boolean compressOnClose) {
        if (mDelegate != null) {
            mDelegate.setCompressOnClose(compressOnClose);
        }
    }

    public void destroy() {
        if (mDelegate != null) {
            mDelegate.destroy();
            mDelegate = null;
        }
    }

    public View getView() {
        return mHostView;
    }

    private PlayerCompositorDelegate.Factory getCompositorDelegateFactory() {
        return (sCompositorDelegateFactoryForTesting != null) ? sCompositorDelegateFactoryForTesting
                                                              : PlayerCompositorDelegateImpl::new;
    }

    @VisibleForTesting
    public boolean checkRequiredBitmapsLoadedForTest() {
        return mRootFrameCoordinator.checkRequiredBitmapsLoadedForTest();
    }

    @VisibleForTesting
    public static void overrideCompositorDelegateFactoryForTesting(
            PlayerCompositorDelegate.Factory factory) {
        sCompositorDelegateFactoryForTesting = factory;
    }
}
