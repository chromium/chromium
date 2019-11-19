// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import java.util.Arrays;

/**
 * This class represents an individual frame in the context of Paint Preview. A frame can be either
 * an embedded iframe, or the root frame of the web page.
 * Each frame has a GUID, content width and height.
 * Optionally, a frame can have other frames (iframes) as its children. or sub-frames.
 */
class PaintPreviewFrame {
    private long mGuid;
    // The content size of this frame. In native, this is represented as 'scroll extent'.
    private int mContentWidth, mContentHeight;
    // Other frames that this frame embeds, its sub-frames.
    private PaintPreviewFrame[] mSubFrames;
    // The coordinates of the sub-frames relative to this frame.
    private Rect[] mSubFrameClips;

    PaintPreviewFrame(long guid, int contentWidth, int contentHeight) {
        mGuid = guid;
        mContentWidth = contentWidth;
        mContentHeight = contentHeight;
    }

    private PaintPreviewFrame(long guid, int contentWidth, int contentHeight,
            PaintPreviewFrame[] subFrames, Rect[] subFrameClips) {
        mGuid = guid;
        mContentWidth = contentWidth;
        mContentHeight = contentHeight;
        mSubFrames = subFrames;
        mSubFrameClips = subFrameClips;
    }

    void setSubFrames(PaintPreviewFrame[] subFrames) {
        mSubFrames = subFrames;
    }

    void setSubFrameClips(Rect[] subFrameClips) {
        mSubFrameClips = subFrameClips;
    }

    long getGuid() {
        return mGuid;
    }

    int getContentWidth() {
        return mContentWidth;
    }

    int getContentHeight() {
        return mContentHeight;
    }

    PaintPreviewFrame[] getSubFrames() {
        return mSubFrames;
    }

    Rect[] getSubFrameClips() {
        return mSubFrameClips;
    }

    @Override
    public boolean equals(Object obj) {
        if (getClass() != obj.getClass()) return false;

        PaintPreviewFrame other = (PaintPreviewFrame) obj;
        if (this.mGuid != other.mGuid) return false;

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

    @VisibleForTesting
    static PaintPreviewFrame createInstanceForTest(long guid, int contentWidth, int contentHeight,
            PaintPreviewFrame[] subFrames, Rect[] subFrameClips) {
        return new PaintPreviewFrame(guid, contentWidth, contentHeight, subFrames, subFrameClips);
    }
}
