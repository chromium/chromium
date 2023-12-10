// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;

import org.chromium.base.UnguessableToken;

import java.util.Arrays;

/**
 * This class represents an individual frame in the context of Paint Preview. A frame can be either
 * an embedded iframe, or the root frame of the web page.
 * Each frame has a GUID, content width and height.
 * Optionally, a frame can have other frames (iframes) as its children. or sub-frames.
 */
class PaintPreviewFrame {
    private UnguessableToken mGuid;
    // The content size of this frame. In native, this is represented as 'scroll extent'.
    private int mContentWidth;
    private int mContentHeight;
    // Other frames that this frame embeds, its sub-frames.
    private PaintPreviewFrame[] mSubFrames;
    // The coordinates of the sub-frames relative to this frame.
    private Rect[] mSubFrameClips;
    // The initial scroll position of this frame.
    private int mInitialScrollX;
    private int mInitialScrollY;

    PaintPreviewFrame(
            UnguessableToken guid,
            int contentWidth,
            int contentHeight,
            int initialScrollX,
            int initialScrollY) {
        mGuid = guid;
        mContentWidth = contentWidth;
        mContentHeight = contentHeight;
        mInitialScrollX = initialScrollX;
        mInitialScrollY = initialScrollY;
    }

    private PaintPreviewFrame(
            UnguessableToken guid,
            int contentWidth,
            int contentHeight,
            int initialScrollX,
            int initialScrollY,
            PaintPreviewFrame[] subFrames,
            Rect[] subFrameClips) {
        mGuid = guid;
        mContentWidth = contentWidth;
        mContentHeight = contentHeight;
        mInitialScrollX = initialScrollX;
        mInitialScrollY = initialScrollY;
        mSubFrames = subFrames;
        mSubFrameClips = subFrameClips;
    }

    void setSubFrames(PaintPreviewFrame[] subFrames) {
        mSubFrames = subFrames;
    }

    void setSubFrameClips(Rect[] subFrameClips) {
        mSubFrameClips = subFrameClips;
    }

    UnguessableToken getGuid() {
        return mGuid;
    }

    int getContentWidth() {
        return mContentWidth;
    }

    int getContentHeight() {
        return mContentHeight;
    }

    int getInitialScrollX() {
        return mInitialScrollX;
    }

    int getInitialScrollY() {
        return mInitialScrollY;
    }

    PaintPreviewFrame[] getSubFrames() {
        return mSubFrames;
    }

    Rect[] getSubFrameClips() {
        return mSubFrameClips;
    }

    /**
     *
     * @param checkDirectChildren Should direct children of this frame be considered.
     * @return Whether this frame has any scrollable descendants.
     */
    boolean hasScrollableDescendants(boolean checkDirectChildren) {
        if (mSubFrameClips == null || mSubFrames == null) {
            return false;
        }

        for (int i = 0; i < mSubFrames.length; i++) {
            PaintPreviewFrame subFrame = mSubFrames[i];
            Rect subFrameClip = mSubFrameClips[i];
            if (checkDirectChildren) {
                if (subFrame.mContentWidth > subFrameClip.width()
                        || subFrame.mContentHeight > subFrameClip.height()) {
                    return true;
                }
            }
            if (subFrame.hasScrollableDescendants(true)) return true;
        }
        return false;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null || getClass() != obj.getClass()) return false;

        PaintPreviewFrame other = (PaintPreviewFrame) obj;
        if (!this.mGuid.equals(other.mGuid)) return false;

        if (this.mContentHeight != other.mContentHeight) return false;

        if (this.mContentWidth != other.mContentWidth) return false;

        if (!Arrays.equals(this.mSubFrames, other.mSubFrames)) return false;

        if (!Arrays.equals(this.mSubFrameClips, other.mSubFrameClips)) return false;

        return true;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Guid : ");
        sb.append(mGuid);
        sb.append(", ContentWidth : ");
        sb.append(mContentWidth);
        sb.append(", ContentHeight: ");
        sb.append(mContentHeight);
        sb.append(", SubFrames: ");
        sb.append(Arrays.deepToString(mSubFrames));
        sb.append(", SubFrameClips: ");
        sb.append(Arrays.deepToString(mSubFrameClips));
        return sb.toString();
    }

    static PaintPreviewFrame createInstanceForTest(
            UnguessableToken guid,
            int contentWidth,
            int contentHeight,
            int initialScrollX,
            int initialScrollY,
            PaintPreviewFrame[] subFrames,
            Rect[] subFrameClips) {
        return new PaintPreviewFrame(
                guid,
                contentWidth,
                contentHeight,
                initialScrollX,
                initialScrollY,
                subFrames,
                subFrameClips);
    }
}
