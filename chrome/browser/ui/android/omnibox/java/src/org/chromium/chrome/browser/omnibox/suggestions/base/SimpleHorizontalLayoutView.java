// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

/**
 * SimpleHorizontalLayoutView is a fast and specialized horizontal layout view. It is based on a
 * premise that no more than one child view can expand dynamically, while all other views must have
 * fixed, predefined size.
 *
 * <p>Principles of operation:
 *
 * <ul>
 *   <li>Each fixed-size view must come with an associated LayoutParams structure.
 *   <li>The dynamically-sized view must have LayoutParams structure unset.
 *   <li>The height of the view will be the result of measurement of the dynamically sized view.
 * </ul>
 */
public class SimpleHorizontalLayoutView extends ViewGroup {
    /**
     * SimpleHorizontalLayoutView's LayoutParams.
     *
     * <p>These parameters introduce additional value to be used with |width| parameter that
     * identifies object that should occupy remaining space.
     */
    public static class LayoutParams extends ViewGroup.LayoutParams {
        /** Indicates a resizable view. */
        public boolean dynamic;

        public LayoutParams(int width, int height) {
            super(width, height);
        }

        /** Create LayoutParams for a dynamic, resizable view. */
        public static LayoutParams forDynamicView() {
            LayoutParams res = new LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
            res.dynamic = true;
            return res;
        }
    }

    public SimpleHorizontalLayoutView(Context context) {
        super(context);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // Note: We layout children in the following order:
        // - first-to-last in LTR orientation and
        // - last-to-first in RTL orientation.
        final boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        final int increment = isRtl ? -1 : 1;
        final int height = getMeasuredHeight();
        int index = isRtl ? getChildCount() - 1 : 0;

        left = getPaddingLeft();

        for (; index >= 0 && index < getChildCount(); index += increment) {
            View v = getChildAt(index);
            if (v.getVisibility() == GONE) continue;

            int verticalMargin = (height - v.getMeasuredHeight()) / 2;
            if (verticalMargin < 0) verticalMargin = 0;

            v.layout(left, verticalMargin, left + v.getMeasuredWidth(), height - verticalMargin);
            left += v.getMeasuredWidth();
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        final int widthPx = MeasureSpec.getSize(widthSpec);
        int contentViewWidth = widthPx - getPaddingLeft() - getPaddingRight();
        View dynamicView = null;

        // Compute and apply space we can offer to content view.
        for (int index = 0; index < getChildCount(); ++index) {
            View v = getChildAt(index);

            LayoutParams p = (LayoutParams) v.getLayoutParams();

            // Do not take dynamic view into consideration when computing space for it.
            // We identify the dynamic view by its custom width parameter.
            if (p.dynamic) {
                assert dynamicView == null : "Only one dynamically sized view is permitted.";
                dynamicView = v;
                continue;
            }

            if (v.getVisibility() == GONE) continue;
            if (p.width > 0) contentViewWidth -= p.width;
        }

        assert dynamicView != null : "No content view specified";

        // Measure height of the content view given the width constraint.
        dynamicView.measure(
                MeasureSpec.makeMeasureSpec(contentViewWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int heightPx = dynamicView.getMeasuredHeight();
        heightSpec = MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY);

        // Apply measured dimensions to all children.
        for (int index = 0; index < getChildCount(); ++index) {
            View v = getChildAt(index);

            // Avoid calling (expensive) measure on dynamic view twice.
            if (v == dynamicView) continue;

            v.measure(
                    MeasureSpec.makeMeasureSpec(v.getLayoutParams().width, MeasureSpec.EXACTLY),
                    getChildMeasureSpec(heightSpec, 0, v.getLayoutParams().height));
        }

        setMeasuredDimension(
                widthPx,
                MeasureSpec.makeMeasureSpec(
                        heightPx + getPaddingTop() + getPaddingBottom(), MeasureSpec.EXACTLY));
    }
}
