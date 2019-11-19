// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.widget.LinearLayout;

/**
 * A LinearLayout that can be constrained to a maximum size or percentage of the screen size.
 *
 * Example:
 *   <org.chromium.chrome.browser.widget.BoundedLinearLayout
 *       xmlns:android="http://schemas.android.com/apk/res/android"
 *       xmlns:app="http://schemas.android.com/apk/res-auto"
 *       android:layout_width="match_parent"
 *       android:layout_height="match_parent"
 *       app:maxWidthLandscape="@dimen/modal_dialog_landscape_max_width"
         app:maxWidthPortrait="@dimen/modal_dialog_portrait_max_width">
 *     ...
 */
public class BoundedLinearLayout extends LinearLayout {
    private static final int NOT_SPECIFIED = -1;

    private TypedValue mMaxWidthLandscape = new TypedValue();
    private TypedValue mMaxWidthPortrait = new TypedValue();

    private final int mMaxHeight;

    /**
     * Constructor for inflating from XML.
     */
    public BoundedLinearLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.BoundedView);
        int maxHeight = a.getDimensionPixelSize(R.styleable.BoundedView_maxHeight, NOT_SPECIFIED);

        a.getValue(R.styleable.BoundedView_maxWidthLandscape, mMaxWidthLandscape);
        a.getValue(R.styleable.BoundedView_maxWidthPortrait, mMaxWidthPortrait);

        a.recycle();

        mMaxHeight = maxHeight <= 0 ? NOT_SPECIFIED : maxHeight;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final DisplayMetrics metrics = getContext().getResources().getDisplayMetrics();
        final boolean isPortrait = metrics.widthPixels < metrics.heightPixels;

        // Limit the width.
        int widthSize = MeasureSpec.getSize(widthMeasureSpec);
        final TypedValue tv = isPortrait ? mMaxWidthPortrait : mMaxWidthLandscape;
        if (tv.type != TypedValue.TYPE_NULL) {
            int maxWidthPixel = NOT_SPECIFIED;
            if (tv.type == TypedValue.TYPE_DIMENSION) {
                maxWidthPixel = (int) tv.getDimension(metrics);
            } else if (tv.type == TypedValue.TYPE_FRACTION) {
                maxWidthPixel = (int) tv.getFraction(metrics.widthPixels, metrics.widthPixels);
            }

            if (widthSize > maxWidthPixel && maxWidthPixel > 0) {
                widthMeasureSpec = makeMeasureSpec(widthMeasureSpec, maxWidthPixel);
            }
        }

        // Limit the height.
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        if (mMaxHeight != NOT_SPECIFIED && heightSize > mMaxHeight) {
            heightMeasureSpec = makeMeasureSpec(heightMeasureSpec, mMaxHeight);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private int makeMeasureSpec(int measureSpec, int maxPixel) {
        int mode = MeasureSpec.getMode(measureSpec);
        if (mode == MeasureSpec.UNSPECIFIED) mode = MeasureSpec.AT_MOST;
        return MeasureSpec.makeMeasureSpec(maxPixel, mode);
    }
}
