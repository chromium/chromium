// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.accessibility;

import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.ViewStructure;

import org.chromium.components.paintpreview.player.frame.PlayerFrameCoordinator;
import org.chromium.components.paintpreview.player.frame.PlayerFrameViewport;
import org.chromium.content.browser.accessibility.AccessibilityDelegate;
import org.chromium.content_public.browser.WebContents;

/** Implementation of {@link AccessibilityDelegate} based on the Paint Preview Player. */
public class PlayerAccessibilityDelegate implements AccessibilityDelegate {
    private final PlayerFrameCoordinator mRootCoordinator;
    private final long mNativeAxTree;
    private final PlayerAccessibilityCoordinatesImpl mPlayerAccessibilityCoordinates;

    public PlayerAccessibilityDelegate(
            PlayerFrameCoordinator coordinator, long nativeAxTree, Size constantOffset) {
        mRootCoordinator = coordinator;
        mNativeAxTree = nativeAxTree;
        mPlayerAccessibilityCoordinates =
                new PlayerAccessibilityCoordinatesImpl(
                        mRootCoordinator.getViewportForAccessibility(),
                        mRootCoordinator.getContentSizeForAccessibility(),
                        constantOffset);
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
    public void requestAccessibilitySnapshot(ViewStructure root, Runnable doneCallback) {
        // Not implemented. This is used to support Assistant reading the screen,
        // which isn't important to support during the short window of time when the
        // Player is active.
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
        private final Size mConstantOffset;

        PlayerAccessibilityCoordinatesImpl(
                PlayerFrameViewport viewport, Size contentSize, Size constantOffset) {
            mViewport = viewport;
            mContentSize = contentSize;
            mConstantOffset = constantOffset;
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
        public int getLastFrameViewportWidthPixInt() {
            return mViewport.getWidth();
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
            return rect.left + (mConstantOffset == null ? 0 : mConstantOffset.getWidth());
        }

        @Override
        public float getScrollY() {
            Rect rect = mViewport.asRect();
            return rect.top + (mConstantOffset == null ? 0 : mConstantOffset.getHeight());
        }
    }
}
