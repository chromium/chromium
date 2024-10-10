// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

/** SimpleVerticalLayoutView is a fast and specialized vertical layout view. */
public class SimpleVerticalLayoutView extends ViewGroup {
    public SimpleVerticalLayoutView(Context context) {
        super(context);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int base = getPaddingTop();
        left = getPaddingLeft();
        for (int index = 0; index < getChildCount(); index++) {
            View v = getChildAt(index);
            if (v.getVisibility() == GONE) continue;

            v.layout(left, base, left + v.getMeasuredWidth(), base + v.getMeasuredHeight());
            base += v.getMeasuredHeight();
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        final int widthPx = MeasureSpec.getSize(widthSpec);
        final int viewWidth = widthPx - getPaddingLeft() - getPaddingRight();

        int totalHeight = 0;
        // Apply measured dimensions to all children.
        for (int index = 0; index < getChildCount(); ++index) {
            View v = getChildAt(index);
            v.measure(
                    MeasureSpec.makeMeasureSpec(viewWidth, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            totalHeight += v.getMeasuredHeight();
        }

        setMeasuredDimension(
                widthPx,
                MeasureSpec.makeMeasureSpec(
                        totalHeight + getPaddingTop() + getPaddingBottom(), MeasureSpec.EXACTLY));
    }
}
