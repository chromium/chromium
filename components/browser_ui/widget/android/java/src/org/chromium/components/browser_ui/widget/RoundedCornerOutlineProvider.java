// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.graphics.Outline;
import android.view.View;
import android.view.ViewOutlineProvider;

import androidx.annotation.VisibleForTesting;

/**
 * A custom {@link ViewOutlineProvider} that is able to render content with rounded off corners. The
 * instance guarantees that only the actual bounds of the view are rounded, ie. if the content does
 * not stretch into the corners, it won't be rounded. This class can be applied to any view,
 * including containers.
 *
 * <p>Affect background/foreground colors alike, as well as select/focus states etc.
 *
 * <p>To apply: <code>
 *     myView.setOutlineProvider(new RoundedCornerOutlineProvider(r));
 *     myView.setClipToOutline(true);
 * </code>
 */
public class RoundedCornerOutlineProvider extends ViewOutlineProvider {
    /** Radius of each corner. */
    private int mRadius;

    private boolean mRoundLeftEdge;
    private boolean mRoundTopEdge;
    private boolean mRoundRightEdge;
    private boolean mRoundBottomEdge;
    private boolean mClipPaddedArea;

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
        var left = 0;
        var top = 0;
        var right = view.getWidth();
        var bottom = view.getHeight();

        if (mClipPaddedArea) {
            left += view.getPaddingLeft();
            top += view.getPaddingTop();
            right -= view.getPaddingRight();
            bottom -= view.getPaddingBottom();
        }

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
     *
     * @param radius The radius to apply to round rectangle corners.
     */
    public void setRadius(int radius) {
        mRadius = radius;
    }

    /**
     * Specify which region of view to clip.
     *
     * @param clipPaddedArea when true, area within the view will be clipped, otherwise the entire
     *     view will be clipped.
     */
    public void setClipPaddedArea(boolean clipPaddedArea) {
        mClipPaddedArea = clipPaddedArea;
    }

    /** Override edges that receive rounding. */
    public void setRoundingEdges(
            boolean leftEdge, boolean topEdge, boolean rightEdge, boolean bottomEdge) {
        mRoundLeftEdge = leftEdge;
        mRoundTopEdge = topEdge;
        mRoundRightEdge = rightEdge;
        mRoundBottomEdge = bottomEdge;
    }

    /** Returns the radius used to round the view. */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public int getRadiusForTesting() {
        return mRadius;
    }

    /** Returns whether the corners around the top edge are rounded. */
    public boolean isTopEdgeRounded() {
        return mRoundTopEdge;
    }

    /** Returns whether the corners around the bottom edge are rounded. */
    public boolean isBottomEdgeRounded() {
        return mRoundBottomEdge;
    }
}
