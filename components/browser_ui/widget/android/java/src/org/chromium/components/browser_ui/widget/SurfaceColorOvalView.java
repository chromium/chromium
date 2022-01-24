// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.components.browser_ui.styles.ChromeColors;

/** Simple view class with a Surface-N colored oval/circle background. */
public class SurfaceColorOvalView extends View {
    /**
     * Constructor that is called when inflating a view from XML.
     * @param context The Context the view is running in, through which it can access the current
     *        theme, resources, etc.
     * @param attrs The attributes of the XML tag that is inflating the view.
     */
    public SurfaceColorOvalView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        TypedArray typedArray =
                context.obtainStyledAttributes(attrs, R.styleable.SurfaceColorOvalView, 0, 0);
        @Px
        float elevation =
                typedArray.getDimension(R.styleable.SurfaceColorOvalView_surfaceElevation, 0);
        @ColorInt
        int color = ChromeColors.getSurfaceColor(context, elevation);
        typedArray.recycle();

        GradientDrawable gradientDrawable = new GradientDrawable();
        gradientDrawable.setColor(color);
        gradientDrawable.setShape(GradientDrawable.OVAL);
        setBackground(gradientDrawable);
    }
}
