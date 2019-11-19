// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.widget.FrameLayout;

/**
 * A layout for displaying a View with padding, borders, and a maximum width and/or height. E.g.:
 *
 *   <org.chromium.chrome.browser.ui.widget.PaddedFrameLayout
 *       chrome:maxChildWidth="200dp"
 *       chrome:maxChildHeight="400dp">
 *       ... contents here ...
 *   </org.chromium.chrome.browser.ui.widget.PaddedFrameLayout>
 */
public class PaddedFrameLayout extends FrameLayout {
    // Value for mMaxChildWidth or mMaxChildHeight to specify that the width or height should
    // not be constrained.
    private static final int NO_MAX_SIZE = -1;

    private int mMaxChildWidth;
    private int mMaxChildHeight;
    private int mHorizontalPadding;
    private int mTopPadding;
    private int mBottomPadding;

    /**
     * Constructor for inflating from XML.
     */
    public PaddedFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.PaddedFrameLayout);
        mMaxChildWidth =
                a.getDimensionPixelSize(R.styleable.PaddedFrameLayout_maxChildWidth, NO_MAX_SIZE);
        mMaxChildHeight =
                a.getDimensionPixelSize(R.styleable.PaddedFrameLayout_maxChildHeight, NO_MAX_SIZE);
        a.recycle();
    }

    protected void setMaxChildWidth(int maxChildWidth) {
        mMaxChildWidth = maxChildWidth;
    }

    protected void setMaxChildHeight(int maxChildHeight) {
        mMaxChildHeight = maxChildHeight;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHorizontalPadding = getPaddingLeft();
        mTopPadding = getPaddingTop();
        mBottomPadding = getPaddingBottom();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int horizontalPadding = mHorizontalPadding;
        if (mMaxChildWidth != NO_MAX_SIZE) {
            // Increase padding if needed to ensure children are no wider than mMaxChildWidth.
            int childWidth = MeasureSpec.getSize(widthMeasureSpec) - 2 * mHorizontalPadding;
            int excessWidth = childWidth - mMaxChildWidth;
            if (excessWidth > 0) {
                horizontalPadding += excessWidth / 2;
            }
        }

        int topPadding = mTopPadding;
        int bottomPadding = mBottomPadding;
        if (mMaxChildHeight != NO_MAX_SIZE) {
            // Increase padding if needed to ensure children are no taller than mMaxChildHeight.
            int childHeight =
                    MeasureSpec.getSize(heightMeasureSpec) - (mTopPadding + mBottomPadding);
            int excessHeight = childHeight - mMaxChildHeight;
            if (excessHeight > 0) {
                topPadding += excessHeight / 2;
                bottomPadding += excessHeight / 2;
            }
        }
        setPadding(horizontalPadding, topPadding, horizontalPadding, bottomPadding);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
