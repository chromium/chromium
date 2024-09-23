// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.components.paintpreview.player.accessibility.PlayerAccessibilityDelegate;
import org.chromium.components.paintpreview.player.frame.PlayerFrameCoordinator;
import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;
import org.chromium.content_public.browser.WebContentsAccessibility;
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
        /** Called when the compositor cannot be successfully initialized. */
        void onCompositorError(@CompositorStatus int status);

        /**
         * Called when the view is ready. Will not be called if onCompositorError is called prior to
         * the view being ready.
         */
        void onViewReady();

        /** Called when the first paint event happens. */
        void onFirstPaint();

        /** Called when the use interacts with the paint preview. */
        void onUserInteraction();

        /** Called when a frustrated behavior is detected. */
        void onUserFrustration();

        /** Called when the a pull to refresh gesture is performed. */
        void onPullToRefresh();

        /** Called with a url to trigger a navigation. */
        void onLinkClick(GURL url);

        /** @return Whether accessibility is currently enabled. */
        boolean isAccessibilityEnabled();

        /** Called when accessibility for paint preview cannot be provided. */
        void onAccessibilityNotSupported();
    }

    private static PlayerCompositorDelegate.Factory sCompositorDelegateFactory =
            new CompositorDelegateFactory();

    private Context mContext;
    private PlayerCompositorDelegate mDelegate;
    private PaintPreviewFrame mRootFrameData;
    private PlayerFrameCoordinator mRootFrameCoordinator;
    private FrameLayout mHostView;
    private static final String sInitEvent = "paint_preview PlayerManager init";
    private PlayerSwipeRefreshHandler mPlayerSwipeRefreshHandler;
    private PlayerGestureListener mPlayerGestureListener;
    private boolean mIgnoreInitialScrollOffset;
    private Listener mListener;
    private long mNativeAxTree;
    private PlayerAccessibilityDelegate mAccessibilityDelegate;
    private WebContentsAccessibilityImpl mWebContentsAccessibility;

    // The minimum ratio value of a sub-frame's area to its parent, for the sub-frame to be
    // considered 'large'.
    private static final float LARGE_SUB_FRAME_RATIO = .8f;
    // The maximum scroll extent value that is allowed for a frame to be considered non-scrollable,
    // as a ratio of the viewport height.
    private static final float SCROLLABLE_FRAME_LENIENCY_RATIO = .1f;

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
    public PlayerManager(
            GURL url,
            Context context,
            NativePaintPreviewServiceProvider nativePaintPreviewServiceProvider,
            String directoryKey,
            @NonNull Listener listener,
            int backgroundColor,
            boolean ignoreInitialScrollOffset) {
        TraceEvent.begin("PlayerManager");
        TraceEvent.startAsync(sInitEvent, hashCode());
        mContext = context;
        mListener = listener;
        mIgnoreInitialScrollOffset = ignoreInitialScrollOffset;

        // This calls into native to set up the compositor.
        mDelegate =
                getCompositorDelegateFactory()
                        .create(
                                nativePaintPreviewServiceProvider,
                                url,
                                directoryKey,
                                false,
                                this::onCompositorReady,
                                mListener::onCompositorError);

        // TODO(crbug.com/40190158): Consider making these parts of setup deferred as these objects
        // aren't needed immediately and appear to be the slowest part of PlayerManager init.
        mPlayerSwipeRefreshHandler =
                new PlayerSwipeRefreshHandler(mContext, mListener::onPullToRefresh);
        mPlayerGestureListener =
                new PlayerGestureListener(
                        mListener::onLinkClick,
                        mListener::onUserInteraction,
                        mListener::onUserFrustration);

        // Set up the HostView to avoid partial loads looking choppy. Ensure it draws so that the
        // container has a defined height immediately. Otherwise on emulators onDraw might not be
        // called successfully.
        mHostView = new FrameLayout(mContext);
        mHostView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mHostView.setBackgroundColor(backgroundColor);
        mHostView.setWillNotDraw(false);
        mHostView.postInvalidate();

        TraceEvent.end("PlayerManager");
    }

    public void setAcceptUserInput(boolean acceptUserInput) {
        if (mRootFrameCoordinator == null) return;

        mRootFrameCoordinator.setAcceptUserInput(acceptUserInput);
    }

    /** @return Current scroll position of the main frame. null if the player is not initialized. */
    public Point getScrollPosition() {
        if (mRootFrameCoordinator == null) return null;

        Point rootScrollPosition = mRootFrameCoordinator.getScrollPosition();
        Point rootOffset = mDelegate.getRootFrameOffsets();
        rootOffset.offset(rootScrollPosition.x, rootScrollPosition.y);
        return rootOffset;
    }

    /** @return Current scale. 0 if the player is not initialized. */
    public float getScale() {
        if (mRootFrameCoordinator == null) return 0f;

        return mRootFrameCoordinator.getScale();
    }

    /**
     * Called by {@link PlayerCompositorDelegateImpl} when the compositor is initialized. This
     * method initializes a sub-component for each frame and adds the view for the root frame to
     * {@link #mHostView}.
     */
    private void onCompositorReady(
            UnguessableToken rootFrameGuid,
            UnguessableToken[] frameGuids,
            int[] frameContentSize,
            int[] scrollOffsets,
            int[] subFramesCount,
            UnguessableToken[] subFrameGuids,
            int[] subFrameClipRects,
            float pageScaleFactor,
            long nativeAxTree) {
        TraceEvent.begin("PlayerManager.onCompositorReady");
        mRootFrameData =
                buildFrameTreeHierarchy(
                        rootFrameGuid,
                        frameGuids,
                        frameContentSize,
                        scrollOffsets,
                        subFramesCount,
                        subFrameGuids,
                        subFrameClipRects,
                        mIgnoreInitialScrollOffset);

        float initialScaleFactor =
                Math.max(
                        pageScaleFactor,
                        mHostView.getWidth() / ((float) mRootFrameData.getContentWidth()));
        mRootFrameCoordinator =
                new PlayerFrameCoordinator(
                        mContext,
                        mDelegate,
                        mRootFrameData.getGuid(),
                        mRootFrameData.getContentWidth(),
                        mRootFrameData.getContentHeight(),
                        mRootFrameData.getInitialScrollX(),
                        mRootFrameData.getInitialScrollY(),
                        initialScaleFactor,
                        true,
                        mPlayerSwipeRefreshHandler,
                        mPlayerGestureListener,
                        mListener::onFirstPaint,
                        mListener::isAccessibilityEnabled,
                        this::initializeAccessibility);
        buildSubFrameCoordinators(mRootFrameCoordinator, mRootFrameData);
        mHostView.addView(
                mRootFrameCoordinator.getView(),
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        if (mPlayerSwipeRefreshHandler != null) {
            mHostView.addView(mPlayerSwipeRefreshHandler.getView());
        }

        mNativeAxTree = nativeAxTree;
        TraceEvent.finishAsync(sInitEvent, hashCode());
        mListener.onViewReady();
        TraceEvent.end("PlayerManager.onCompositorReady");
    }

    /**
     * Attempts to initialize accessibility support for the player. The conditional logic exists
     * because of the lack of accessibility support for paint previews with multiple scrollable
     * sub-frames. The following cases can happen for a given paint preview:
     * - If the root frame has no scrollable sub-frames, accessibility support will be added to it.
     * - If the root frame has any scrollable sub-frames that are not its direct children, we can't
     * add accessibility support.
     * - If the root frame is scrollable and has any direct or indirect scrollable sub-frames,
     * we can't have accessibility support.
     * - If the root frame is not scrollable and has one large direct scrollable sub-frame (which
     * is the case for AMP), it adds accessibility support for that sub-frame.
     * - In any other case, we can't add accessibility support.
     */
    private void initializeAccessibility() {
        // Early exit if already closed.
        if (mRootFrameCoordinator == null
                || mRootFrameCoordinator.getViewportForAccessibility() == null) {
            mListener.onAccessibilityNotSupported();
            return;
        }

        if (mNativeAxTree == 0) {
            mListener.onAccessibilityNotSupported();
            return;
        }

        if (mRootFrameData.hasScrollableDescendants(false)) {
            // If there are any scrollable sub-frames that are not direct children of root frame,
            // we can't add accessibility support regardless of root frame's scrollability.
            mListener.onAccessibilityNotSupported();
            return;
        }

        if (!mRootFrameData.hasScrollableDescendants(true)) {
            // In the absence of scrollable sub-frames, we can add accessibility support to the root
            // frame.
            mAccessibilityDelegate =
                    new PlayerAccessibilityDelegate(mRootFrameCoordinator, mNativeAxTree, null);
            mWebContentsAccessibility =
                    WebContentsAccessibilityImpl.fromDelegate(mAccessibilityDelegate);
            mRootFrameCoordinator.getView().setWebContentsAccessibility(mWebContentsAccessibility);
            return;
        }

        final float mainFrameScale = mRootFrameCoordinator.getViewportForAccessibility().getScale();
        final int mainFrameViewportHeight =
                mRootFrameCoordinator.getViewportForAccessibility().getHeight();
        final float mainFrameScrollAmountPx =
                (mainFrameScale * mRootFrameData.getContentHeight()) - mainFrameViewportHeight;
        final float mainFrameScrollLeniencyPx =
                SCROLLABLE_FRAME_LENIENCY_RATIO * mainFrameViewportHeight;
        final boolean isMainFrameScrollable = mainFrameScrollAmountPx > mainFrameScrollLeniencyPx;
        if (isMainFrameScrollable) {
            // We cannot have accessibility support if we have scrollable sub-frames as well as a
            // scrollable main frame.
            mListener.onAccessibilityNotSupported();
            return;
        }

        // If the main frame is not scrollable and we have exactly 1 large scrollable sub-frame
        // (which is the case in AMPs), we can add accessibility support.
        int scrollableSubFrameIndex = indexOfLargeScrollableSubFrame();
        if (scrollableSubFrameIndex == -1) {
            // There were either more than 1 scrollable sub-frames, or the scrollable sub-frame
            // was not large enough.
            mListener.onAccessibilityNotSupported();
            return;
        }

        PlayerFrameCoordinator scrollableSubFrame =
                mRootFrameCoordinator.getSubFrameForAccessibility(scrollableSubFrameIndex);
        if (scrollableSubFrame == null) {
            mListener.onAccessibilityNotSupported();
            return;
        }

        Size subFrameOffset =
                new Size(
                        mRootFrameData.getSubFrameClips()[scrollableSubFrameIndex].left,
                        mRootFrameData.getSubFrameClips()[scrollableSubFrameIndex].top);
        mAccessibilityDelegate =
                new PlayerAccessibilityDelegate(scrollableSubFrame, mNativeAxTree, subFrameOffset);
        mWebContentsAccessibility =
                WebContentsAccessibilityImpl.fromDelegate(mAccessibilityDelegate);
        scrollableSubFrame.getView().setWebContentsAccessibility(mWebContentsAccessibility);
    }

    /**
     * Searches for a large scrollable sub-frame. Only returns a valid index if there is only one
     * scrollable direct sub-frame, and that sub-frame is sufficiently large (80% of main frame).
     */
    private int indexOfLargeScrollableSubFrame() {
        Rect mainFrameViewPort = mRootFrameCoordinator.getViewportForAccessibility().asRect();
        int scrollableSubFrameIndex = -1;
        boolean hasLargeScrollableSubFrame = false;
        for (int i = 0; i < mRootFrameData.getSubFrames().length; i++) {
            PaintPreviewFrame subFrame = mRootFrameData.getSubFrames()[i];
            Rect subFrameClip = mRootFrameData.getSubFrameClips()[i];
            if (subFrame.getContentWidth() > subFrameClip.width()
                    || subFrame.getContentHeight() > subFrameClip.width()) {
                if (scrollableSubFrameIndex != -1) {
                    // This is the second scrollable sub-frame. We can't have accessibility support.
                    scrollableSubFrameIndex = -1;
                    break;
                }
                scrollableSubFrameIndex = i;
                float subFrameArea = subFrameClip.width() * subFrameClip.height();
                float mainFrameArea = mainFrameViewPort.width() * mainFrameViewPort.height();
                if (subFrameArea / mainFrameArea > LARGE_SUB_FRAME_RATIO) {
                    hasLargeScrollableSubFrame = true;
                }
            }
        }
        return hasLargeScrollableSubFrame ? scrollableSubFrameIndex : -1;
    }

    /**
     * This method builds a hierarchy of {@link PaintPreviewFrame}s from primitive variables that
     * originate from native. Detailed explanation of the parameters can be found in {@link
     * PlayerCompositorDelegateImpl#onCompositorReady}.
     *
     * @return The root {@link PaintPreviewFrame}
     */
    @VisibleForTesting
    static PaintPreviewFrame buildFrameTreeHierarchy(
            UnguessableToken rootFrameGuid,
            UnguessableToken[] frameGuids,
            int[] frameContentSize,
            int[] scrollOffsets,
            int[] subFramesCount,
            UnguessableToken[] subFrameGuids,
            int[] subFrameClipRects,
            boolean ignoreInitialScrollOffset) {
        Map<UnguessableToken, PaintPreviewFrame> framesMap = new HashMap<>();
        for (int i = 0; i < frameGuids.length; i++) {
            int initalScrollX = ignoreInitialScrollOffset ? 0 : scrollOffsets[i * 2];
            int initalScrollY = ignoreInitialScrollOffset ? 0 : scrollOffsets[(i * 2) + 1];
            framesMap.put(
                    frameGuids[i],
                    new PaintPreviewFrame(
                            frameGuids[i],
                            frameContentSize[i * 2],
                            frameContentSize[(i * 2) + 1],
                            initalScrollX,
                            initalScrollY));
        }

        int subFrameIdIndex = 0;
        for (int i = 0; i < frameGuids.length; i++) {
            PaintPreviewFrame currentFrame = framesMap.get(frameGuids[i]);
            int currentFrameSubFrameCount = subFramesCount[i];
            PaintPreviewFrame[] subFrames = new PaintPreviewFrame[currentFrameSubFrameCount];
            Rect[] subFrameClips = new Rect[currentFrameSubFrameCount];
            for (int subFrameIndex = 0;
                    subFrameIndex < currentFrameSubFrameCount;
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
            PlayerFrameCoordinator childCoordinator =
                    new PlayerFrameCoordinator(
                            mContext,
                            mDelegate,
                            childFrame.getGuid(),
                            childFrame.getContentWidth(),
                            childFrame.getContentHeight(),
                            childFrame.getInitialScrollX(),
                            childFrame.getInitialScrollY(),
                            0f,
                            false,
                            null,
                            mPlayerGestureListener,
                            null,
                            null,
                            null);
            buildSubFrameCoordinators(childCoordinator, childFrame);
            frameCoordinator.addSubFrame(childCoordinator, frame.getSubFrameClips()[i]);
        }
    }

    public boolean supportsAccessibility() {
        return mWebContentsAccessibility != null;
    }

    public void setCompressOnClose(boolean compressOnClose) {
        if (mDelegate != null) {
            mDelegate.setCompressOnClose(compressOnClose);
        }
    }

    public void destroy() {
        if (mWebContentsAccessibility != null) {
            mRootFrameCoordinator.getView().setWebContentsAccessibility(null);
            mWebContentsAccessibility.destroy();
            mWebContentsAccessibility = null;
            mAccessibilityDelegate = null;
        }
        if (mDelegate != null) {
            mDelegate.destroy();
            mDelegate = null;
        }
        if (mRootFrameCoordinator != null) {
            mRootFrameCoordinator.destroy();
            mRootFrameCoordinator = null;
        }
    }

    public View getView() {
        return mHostView;
    }

    static class CompositorDelegateFactory implements PlayerCompositorDelegate.Factory {
        @Override
        public PlayerCompositorDelegate create(
                NativePaintPreviewServiceProvider service,
                GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return new PlayerCompositorDelegateImpl(
                    service,
                    0,
                    url,
                    directoryKey,
                    mainFrameMode,
                    compositorListener,
                    compositorErrorCallback);
        }

        @Override
        public PlayerCompositorDelegate createForCaptureResult(
                NativePaintPreviewServiceProvider service,
                long nativeCaptureResultPtr,
                GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return new PlayerCompositorDelegateImpl(
                    service,
                    nativeCaptureResultPtr,
                    url,
                    directoryKey,
                    mainFrameMode,
                    compositorListener,
                    compositorErrorCallback);
        }
    }

    private PlayerCompositorDelegate.Factory getCompositorDelegateFactory() {
        return sCompositorDelegateFactory;
    }

    public boolean checkRequiredBitmapsLoadedForTest() {
        return mRootFrameCoordinator.checkRequiredBitmapsLoadedForTest();
    }

    public WebContentsAccessibility getWebContentsAccessibilityForTesting() {
        return mWebContentsAccessibility;
    }

    public static void overrideCompositorDelegateFactoryForTesting(
            PlayerCompositorDelegate.Factory factory) {
        if (factory == null) {
            sCompositorDelegateFactory = new CompositorDelegateFactory();
            return;
        }
        sCompositorDelegateFactory = factory;
    }
}
