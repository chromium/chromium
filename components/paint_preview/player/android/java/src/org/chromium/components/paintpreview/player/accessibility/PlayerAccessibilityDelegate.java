// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.accessibility;

import android.graphics.Rect;
import android.os.Handler;
import android.util.Size;
import android.view.View;

import org.chromium.components.paintpreview.player.frame.PlayerFrameCoordinator;
import org.chromium.components.paintpreview.player.frame.PlayerFrameViewport;
import org.chromium.content.browser.accessibility.AccessibilityDelegate;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.WebContents;

/**
 * Implementation of {@link AccessibilityDelegate} based on the Paint Preview Player.
 */
public class PlayerAccessibilityDelegate implements AccessibilityDelegate {
    private final PlayerFrameCoordinator mRootCoordinator;
    private final long mNativeAxTree;
    private final PlayerAccessibilityCoordinatesImpl mPlayerAccessibilityCoordinates;

    public PlayerAccessibilityDelegate(PlayerFrameCoordinator coordinator, long nativeAxTree) {
        mRootCoordinator = coordinator;
        mNativeAxTree = nativeAxTree;
        mPlayerAccessibilityCoordinates = new PlayerAccessibilityCoordinatesImpl(
                mRootCoordinator.getViewportForAccessibility(),
                mRootCoordinator.getContentSizeForAccessibility());
    }

    @Override
    public View getContainerView() {
        return mRootCoordinator.getView();
    }

    @Override
    public String getProductVersion() {
        return null;
    }

    @Override
    public boolean isIncognito() {
        return false;
    }

    @Override
    public WebContents getWebContents() {
        return null;
    }

    @Override
    public AccessibilityCoordinates getAccessibilityCoordinates() {
        return mPlayerAccessibilityCoordinates;
    }

    @Override
    public void requestAccessibilitySnapshot(AccessibilitySnapshotCallback callback) {
        new Handler().post(() -> {
            AccessibilitySnapshotNode rootNode =
                    PlayerAccessibilitySnapshotHelper.getJavaAccessibilitySnapshotNode(
                            mNativeAxTree);
            callback.onAccessibilitySnapshot(rootNode);
        });
    }

    @Override
    public long getNativeAXTree() {
        return mNativeAxTree;
    }

    @Override
    public void setOnScrollPositionChangedCallback(Runnable onScrollCallback) {
        mRootCoordinator.setOnScrollCallbackForAccessibility(onScrollCallback);
    }

    @Override
    public boolean performClick(Rect nodeRect) {
        mRootCoordinator.handleClickForAccessibility(nodeRect.centerX(), nodeRect.centerY(), true);
        return true;
    }

    @Override
    public boolean scrollToMakeNodeVisible(Rect nodeRect) {
        mRootCoordinator.scrollToMakeRectVisibleForAccessibility(nodeRect);
        return true;
    }

    static class PlayerAccessibilityCoordinatesImpl implements AccessibilityCoordinates {
        private final PlayerFrameViewport mViewport;
        private final Size mContentSize;

        PlayerAccessibilityCoordinatesImpl(PlayerFrameViewport viewport, Size contentSize) {
            mViewport = viewport;
            mContentSize = contentSize;
        }

        @Override
        public float fromLocalCssToPix(float css) {
            return css;
        }

        @Override
        public float getContentOffsetYPix() {
            // Offset already accounted for by view position.
            return 0;
        }

        @Override
        public float getScrollXPix() {
            return fromLocalCssToPix(getScrollX());
        }

        @Override
        public float getScrollYPix() {
            return fromLocalCssToPix(getScrollY());
        }

        @Override
        public int getLastFrameViewportHeightPixInt() {
            return mViewport.getHeight();
        }

        @Override
        public float getContentWidthCss() {
            return mContentSize.getWidth();
        }

        @Override
        public float getContentHeightCss() {
            return mContentSize.getHeight();
        }

        @Override
        public float getScrollX() {
            Rect rect = mViewport.asRect();
            return rect.left;
        }

        @Override
        public float getScrollY() {
            Rect rect = mViewport.asRect();
            return rect.top;
        }
    }
}
