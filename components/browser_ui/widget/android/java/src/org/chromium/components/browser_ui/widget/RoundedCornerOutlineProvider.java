// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.graphics.Outline;
import android.view.View;
import android.view.ViewOutlineProvider;

/**
 * A custom {@link ViewOutlineProvider} that is able to render content with rounded off corners.
 * The instance guarantees that only the actual bounds of the view are rounded,
 * ie. if the content does not stretch into the corners, it won't be rounded.
 * This class can be applied to any view, including containers.
 *
 * Affect background/foreground colors alike, as well as select/focus states etc.
 *
 * To apply:
 *     myView.setOutlineProvider(new RoundedCornerOutlineProvider(r));
 *     myView.setClipToOutline(true);
 */
public class RoundedCornerOutlineProvider extends ViewOutlineProvider {
    /** Radius of each corner. */
    private int mRadius;
    private boolean mRoundLeftEdge;
    private boolean mRoundTopEdge;
    private boolean mRoundRightEdge;
    private boolean mRoundBottomEdge;

    public RoundedCornerOutlineProvider() {
        this(0);
    }

    public RoundedCornerOutlineProvider(int radius) {
        setRadius(radius);
        mRoundLeftEdge = true;
        mRoundTopEdge = true;
        mRoundRightEdge = true;
        mRoundBottomEdge = true;
    }

    @Override
    public void getOutline(View view, Outline outline) {
        var left = view.getPaddingLeft();
        var top = view.getPaddingTop();
        var right = view.getWidth() - view.getPaddingRight();
        var bottom = view.getHeight() - view.getPaddingBottom();

        // Grow the rounded area size in the direction where the rounding is not desired.
        if (!mRoundLeftEdge) left -= mRadius;
        if (!mRoundTopEdge) top -= mRadius;
        if (!mRoundRightEdge) right += mRadius;
        if (!mRoundBottomEdge) bottom += mRadius;

        // Apply rounding.
        outline.setRoundRect(left, top, right, bottom, mRadius);
    }

    /**
     * Set the rounding radius.
     * @param radius The radius to apply to round rectangle corners.
     */
    public void setRadius(int radius) {
        mRadius = radius;
    }

    /**
     * Override edges that receive rounding.
     */
    public void setRoundingEdges(
            boolean leftEdge, boolean topEdge, boolean rightEdge, boolean bottomEdge) {
        mRoundLeftEdge = leftEdge;
        mRoundTopEdge = topEdge;
        mRoundRightEdge = rightEdge;
        mRoundBottomEdge = bottomEdge;
    }

    /** Returns the radius used to round the view. */
    public int getRadiusForTesting() {
        return mRadius;
    }

    public boolean isTopEdgeRoundedForTesting() {
        return mRoundTopEdge;
    }

    public boolean isBottomEdgeRoundedForTesting() {
        return mRoundBottomEdge;
    }
}
