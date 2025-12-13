// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.extensions.R;

/**
 * A container for extension action buttons. This container will automatically hide any buttons that
 * overflow the available width.
 */
@NullMarked
class ExtensionActionListContainer extends ViewGroup {
    public ExtensionActionListContainer(Context context) {
        super(context);
    }

    public ExtensionActionListContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public LayoutParams generateLayoutParams(AttributeSet attrs) {
        // This override is needed to support margin attributes.
        return new MarginLayoutParams(getContext(), attrs);
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        // This override is needed to support margin attributes.
        return new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
    }

    @Override
    protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) {
        // This override is needed to support margin attributes.
        return new MarginLayoutParams(p);
    }

    @Override
    protected boolean checkLayoutParams(ViewGroup.LayoutParams p) {
        // This override is needed to support margin attributes.
        return p instanceof MarginLayoutParams;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // The parent passes the window width as the maximum width.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.AT_MOST;
        int remainingWidth =
                MeasureSpec.getSize(widthMeasureSpec)
                        - getResources()
                                .getDimensionPixelSize(R.dimen.extension_toolbar_baseline_width)
                        - getPaddingLeft()
                        - getPaddingRight();

        int contentWidth = 0;
        int contentHeight = 0;

        final int numChildren = getChildCount();
        for (int i = 0; i < numChildren; i++) {
            final View child = getChildAt(i);
            if (child.getVisibility() == GONE) {
                continue;
            }

            // Measure the child to see if it fits.
            measureChildWithMargins(child, widthMeasureSpec, 0, heightMeasureSpec, 0);
            final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
            final int childWidthWithMargins =
                    child.getMeasuredWidth() + lp.leftMargin + lp.rightMargin;
            final int childHeightWithMargins =
                    child.getMeasuredHeight() + lp.topMargin + lp.bottomMargin;

            if (childWidthWithMargins > remainingWidth) {
                // Measure remaining children with zero size.
                final int zeroMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.EXACTLY);
                for (; i < numChildren; i++) {
                    final View overflowChild = getChildAt(i);
                    overflowChild.measure(zeroMeasureSpec, zeroMeasureSpec);
                    assert overflowChild.getMeasuredWidth() == 0;
                }
                break;
            }

            remainingWidth -= childWidthWithMargins;
            contentWidth += childWidthWithMargins;
            contentHeight = Math.max(contentHeight, childHeightWithMargins);
        }

        final int measuredWidth = contentWidth + getPaddingLeft() + getPaddingRight();
        final int measuredHeight = contentHeight + getPaddingTop() + getPaddingBottom();

        setMeasuredDimension(
                resolveSize(measuredWidth, widthMeasureSpec),
                resolveSize(measuredHeight, heightMeasureSpec));
    }

    @Override
    protected void onLayout(
            boolean changed, int offsetLeft, int offsetTop, int offsetRight, int offsetBottom) {
        final boolean rtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        final int dir = rtl ? -1 : 1;
        int currentStart = rtl ? getMeasuredWidth() - getPaddingStart() : getPaddingStart();
        final int fixedTop = getPaddingTop();

        final int numChildren = getChildCount();
        for (int i = 0; i < numChildren; i++) {
            final View child = getChildAt(i);
            if (child.getVisibility() == GONE) {
                continue;
            }

            final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
            final int childTop = fixedTop + lp.topMargin;
            final int childBottom = childTop + child.getMeasuredHeight();

            if (child.getMeasuredWidth() == 0) {
                // Layout this child with zero width, and do not update currentStart even if it has
                // margins.
                child.layout(currentStart, childTop, currentStart, childBottom);
            } else {
                final int childStart = currentStart + lp.getMarginStart() * dir;
                final int childEnd = childStart + child.getMeasuredWidth() * dir;
                final int childLeft = rtl ? childEnd : childStart;
                final int childRight = rtl ? childStart : childEnd;
                child.layout(childLeft, childTop, childRight, childBottom);
                currentStart = childEnd + lp.getMarginEnd() * dir;
            }
        }
    }
}
