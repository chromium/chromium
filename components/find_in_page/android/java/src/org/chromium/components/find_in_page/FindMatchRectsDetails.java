// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.find_in_page;

import android.graphics.RectF;

import org.chromium.build.annotations.NullMarked;

/** Holds the result details of a RequestFindMatchRects reply. */
@NullMarked
public class FindMatchRectsDetails {
    /** Version of the the rects in this result. */
    public final int version;

    /** Rects of the find matches in find-in-page coordinates. */
    public final RectF[] rects;

    /** Rect of the active match in find-in-page coordinates. */
    public final RectF activeRect;

    public FindMatchRectsDetails(int version, int numRects, RectF activeRect) {
        this.version = version;
        this.rects = new RectF[numRects];
        this.activeRect = activeRect;
    }
}
