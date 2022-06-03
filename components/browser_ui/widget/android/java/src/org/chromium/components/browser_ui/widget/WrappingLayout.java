// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;

/**
 * A horizontal layout that can wrap to the next line, if there's not enough space to fit all views.
 * Accounts for padding set on self and margins on children, but uniform spacing between elements
 * can be set through attributes, e.g.:
 *
 *     <org.chromium.components.browser_ui.widget.WrappingLayout
 *         android:id="@+id/wrapping_layout"
 *         android:layout_width="match_parent"
 *         android:layout_height="match_parent"
 *         app:horizontalSpacing="10dp"
 *         app:verticalSpacing="5dp">
 */
public class WrappingLayout extends ViewGroup {
    // The amount of horizontal space to apply to each child view (in pixels), in addition to any
    // margins set by the child.
    private int mHorizontalSpacing;

    // The amount of vertical space to apply to each child view (in pixels), in addition to any
    // margins set by the child.
    private int mVerticalSpacing;

    // The indices of visible child views of this layout. Allocated as a member class to avoid
    // allocations while drawing.
    private ArrayList<Integer> mVisibleChildren = new ArrayList<Integer>();

    public WrappingLayout(Context context) {
        this(context, null);
    }

    public WrappingLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public WrappingLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        TypedArray array = getContext().obtainStyledAttributes(
                attrs, R.styleable.WrappingLayout, defStyleAttr, 0);
        mHorizontalSpacing =
                array.getDimensionPixelSize(R.styleable.WrappingLayout_horizontalSpacing, 0);
        mVerticalSpacing =
                array.getDimensionPixelSize(R.styleable.WrappingLayout_verticalSpacing, 0);
    }

    /**
     * Sets the amount of spacing between views.
     * @param horizontal The amount of horizontal spacing to add (in pixels).
     * @param vertical The amount of vertical spacing to add (in pixels).
     */
    @VisibleForTesting
    protected void setSpacingBetweenViews(int horizontal, int vertical) {
        mHorizontalSpacing = horizontal;
        mVerticalSpacing = vertical;
    }

    @Override
    protected boolean checkLayoutParams(ViewGroup.LayoutParams params) {
        return params instanceof MarginLayoutParams;
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
    }

    @Override
    public LayoutParams generateLayoutParams(AttributeSet attrs) {
        return new MarginLayoutParams(getContext(), attrs);
    }

    @Override
    protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams params) {
        return generateDefaultLayoutParams();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int specModeForWidth = MeasureSpec.getMode(widthMeasureSpec);
        int specModeForHeight = MeasureSpec.getMode(heightMeasureSpec);

        // A wrapping layout must have bounded width to be able to figure out it's actual size. Do
        // not call setMeasuredDimension in order to trigger assert.
        if (specModeForWidth == MeasureSpec.UNSPECIFIED) return;

        measureChildren(widthMeasureSpec, heightMeasureSpec);

        if (specModeForWidth == MeasureSpec.EXACTLY && specModeForHeight == MeasureSpec.EXACTLY) {
            int width = MeasureSpec.getSize(widthMeasureSpec);
            int height = MeasureSpec.getSize(heightMeasureSpec);
            setMeasuredDimension(width, height);
            return;
        }

        // Don't account for padding yet, it will be added at the end.
        int maxWidth =
                MeasureSpec.getSize(widthMeasureSpec) - (getPaddingLeft() + getPaddingRight());

        int measuredWidth = 0;
        int measuredHeight = 0;

        int currentRowWidth = 0;
        // Height of the tallest child in the row, including top and bottom margins.
        int tallestChildInRow = 0;

        // Build an array of indices for only the visible child views. This simplifies calculations
        // because it becomes easy to figure out, for example, if the view we're processing is the
        // last visible view or not.
        for (int i = 0; i < getChildCount(); ++i) {
            View child = getChildAt(i);
            if (child.getVisibility() == GONE) continue;
            mVisibleChildren.add(i);
        }

        int count = mVisibleChildren.size();
        for (int i = 0; i < count; ++i) {
            View child = getChildAt(mVisibleChildren.get(i));
            MarginLayoutParams childLp = (MarginLayoutParams) child.getLayoutParams();

            int childWidth =
                    childLp.getMarginStart() + child.getMeasuredWidth() + childLp.getMarginEnd();
            int childHeight = childLp.topMargin + child.getMeasuredHeight() + childLp.bottomMargin;

            if (currentRowWidth + childWidth <= maxWidth) {
                // This item fits in the current row.
                if (currentRowWidth != 0) currentRowWidth += mHorizontalSpacing;
                currentRowWidth += childWidth;

                tallestChildInRow = Math.max(tallestChildInRow, childHeight);
            } else {
                // This item is too large for the remaining space. Start a new row.
                // |measuredHeight| is increased by the height of the tallest child in the
                // *previous* row (if there is a previous row).
                if (tallestChildInRow != 0) measuredHeight += tallestChildInRow + mVerticalSpacing;

                currentRowWidth = childWidth;
                tallestChildInRow = childHeight;
            }

            measuredWidth = Math.max(measuredWidth, currentRowWidth);

            // If this is the last visible view, make sure to increase the height to account for
            // the last row.
            if (i + 1 == count) measuredHeight += tallestChildInRow;
        }

        // Account for padding again.
        measuredWidth += getPaddingLeft() + getPaddingRight();
        measuredHeight += getPaddingTop() + getPaddingBottom();

        setMeasuredDimension(
                resolveSizeAndState(measureDimension(measuredWidth, getSuggestedMinimumWidth(),
                                            widthMeasureSpec),
                        widthMeasureSpec, 0),
                resolveSizeAndState(measureDimension(measuredHeight, getSuggestedMinimumHeight(),
                                            heightMeasureSpec),
                        heightMeasureSpec, 0));

        mVisibleChildren.clear();
    }

    /**
     * Resolves the actual size, after taking the measure spec into account.
     * @param desiredSize The desired size of the view (in pixels).
     * @param minSize The suggested minimum size (in pixels).
     * @param measureSpec The measure spec to use to determine the actual size.
     * @return The actual size of the view, in pixels.
     */
    private int measureDimension(int desiredSize, int minSize, int measureSpec) {
        int mode = MeasureSpec.getMode(measureSpec);
        int size = MeasureSpec.getSize(measureSpec);

        int result = 0;
        if (mode == MeasureSpec.EXACTLY) {
            result = size;
        } else {
            if (mode == MeasureSpec.AT_MOST) {
                result = Math.min(desiredSize, size);
            } else {
                result = desiredSize;
            }

            result = Math.max(result, minSize);
        }

        return result;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int count = getChildCount();

        int x = getPaddingStart();
        int y = getPaddingTop();

        // Height of the tallest child in the row, including top and bottom margins and
        // mVerticalSpacing (if applicable).
        int tallestChildInRow = 0;

        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;

        for (int i = 0; i < count; i++) {
            View child = getChildAt(i);
            if (child.getVisibility() == GONE) continue;

            MarginLayoutParams childLp = (MarginLayoutParams) child.getLayoutParams();

            int childWidth = child.getMeasuredWidth();
            int childHeight = child.getMeasuredHeight();

            // childLeft and childTop should point to the x,y coordinates of where the view will be
            // drawn.
            int childLeft = x + childLp.getMarginStart();
            int childTop = y + childLp.topMargin;

            boolean firstViewInRow = x == getPaddingStart();

            int widthWithMargins = childLp.getMarginStart() + childWidth + childLp.getMarginEnd();
            int heightWithMargins = childLp.topMargin + childHeight + childLp.bottomMargin;

            if (!firstViewInRow && x + widthWithMargins > getMeasuredWidth()) {
                // We've found a view that should wrap to the next line. Note that the first view in
                // a row can never wrap, otherwise it would leave an empty slot behind it.

                // Reset view coordinates to the start of a new line.
                childLeft = getPaddingStart() + childLp.getMarginStart();
                childTop += tallestChildInRow + mVerticalSpacing;
                tallestChildInRow = heightWithMargins;

                y = childTop - childLp.topMargin;
            } else {
                // We've found a view that fits on the current line (or the allocated space is so
                // small that it won't fit anywhere and it should be drawn truncated).
                tallestChildInRow = Math.max(tallestChildInRow, heightWithMargins);
            }

            int childRight = childLeft + childWidth;
            x = childLeft + childWidth + childLp.getMarginEnd() + mHorizontalSpacing;

            if (isRtl) {
                // When flipping to the RTL side, also swap horizontal padding (childLeft includes
                // paddingLeft but should instead account for paddingRight).
                childRight = getMeasuredWidth() - childLeft;
                childLeft = childRight - childWidth;
            }

            child.layout(childLeft, childTop, childRight, childTop + childHeight);
        }
    }
}
