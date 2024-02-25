// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;
import android.util.Size;

/** Stores data about a frame for testing. */
public class FrameData {
    private final Size mSize;
    private final int[] mLinkRects;
    private final String[] mLinks;
    private final int[] mChildRects;
    private final FrameData[] mChildFrames;

    public FrameData(
            Size size,
            Rect[] linkRects,
            String[] links,
            Rect[] childRects,
            FrameData[] childFrames) {
        mSize = size;

        assert linkRects.length == links.length;
        mLinkRects = flattenRects(linkRects);
        mLinks = links;

        assert childRects.length == childFrames.length;
        mChildRects = flattenRects(childRects);
        mChildFrames = childFrames;
    }

    public int getWidth() {
        return mSize.getWidth();
    }

    public int getHeight() {
        return mSize.getHeight();
    }

    public int[] getFlattenedLinkRects() {
        return mLinkRects;
    }

    public String[] getLinks() {
        return mLinks;
    }

    public int[] getFlattenedChildRects() {
        return mChildRects;
    }

    public FrameData[] getChildFrames() {
        return mChildFrames;
    }

    private int[] flattenRects(Rect[] rects) {
        int flattenedRects[] = new int[rects.length * 4];
        for (int i = 0; i < rects.length; i++) {
            flattenedRects[i * 4] = rects[i].left;
            flattenedRects[i * 4 + 1] = rects[i].top;
            flattenedRects[i * 4 + 2] = rects[i].width();
            flattenedRects[i * 4 + 3] = rects[i].height();
        }
        return flattenedRects;
    }
}
