// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.text;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;

import org.chromium.components.browser_ui.widget.R;

/**
 * A {@link TextView} with support for explicitly sizing and tinting compound drawables.
 *
 * To specify the drawable size, use the {@code drawableWidth} and {@code drawableHeight}
 * attributes. To specify the drawable tint, use the {@code chromeDrawableTint} attribute.
 */
public class TextViewWithCompoundDrawables extends AppCompatTextView {
    private int mDrawableWidth;
    private int mDrawableHeight;
    private ColorStateList mDrawableTint;

    public TextViewWithCompoundDrawables(Context context) {
        this(context, null);
    }

    public TextViewWithCompoundDrawables(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TextViewWithCompoundDrawables(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs, defStyleAttr);
    }

    /**
     * Set the tint color of the compound drawables.
     * @param color The tint color.
     */
    public void setDrawableTintColor(ColorStateList color) {
        mDrawableTint = color;
        setDrawableTint(getCompoundDrawablesRelative());
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();

        if (mDrawableTint != null) {
            setDrawableTint(getCompoundDrawablesRelative());
        }
    }

    @Override
    public void setCompoundDrawablesRelative(@Nullable Drawable start, @Nullable Drawable top,
            @Nullable Drawable end, @Nullable Drawable bottom) {
        Drawable[] drawables = {start, top, end, bottom};
        setDrawableBounds(drawables);

        if (mDrawableTint != null) {
            setDrawableTint(drawables);
        }

        super.setCompoundDrawablesRelative(drawables[0], drawables[1], drawables[2], drawables[3]);
    }

    private void init(Context context, AttributeSet attrs, int defStyleAttr) {
        TypedArray array = context.obtainStyledAttributes(
                attrs, R.styleable.TextViewWithCompoundDrawables, defStyleAttr, 0);

        mDrawableWidth = array.getDimensionPixelSize(
                R.styleable.TextViewWithCompoundDrawables_drawableWidth, -1);
        mDrawableHeight = array.getDimensionPixelSize(
                R.styleable.TextViewWithCompoundDrawables_drawableHeight, -1);
        mDrawableTint = array.getColorStateList(
                R.styleable.TextViewWithCompoundDrawables_chromeDrawableTint);

        array.recycle();

        if (mDrawableWidth <= 0 && mDrawableHeight <= 0 && mDrawableTint == null) return;

        Drawable[] drawables = getCompoundDrawablesRelative();

        setDrawableBounds(drawables);

        if (mDrawableTint != null) setDrawableTint(drawables);

        setCompoundDrawablesRelative(drawables[0], drawables[1], drawables[2], drawables[3]);
    }

    private void setDrawableTint(Drawable[] drawables) {
        for (Drawable drawable : drawables) {
            if (drawable == null) continue;

            drawable.mutate();
            drawable.setColorFilter(
                    mDrawableTint.getColorForState(getDrawableState(), 0), PorterDuff.Mode.SRC_IN);
        }
    }

    private void setDrawableBounds(Drawable[] drawables) {
        for (Drawable drawable : drawables) {
            if (drawable == null) continue;

            if (mDrawableWidth > 0 || mDrawableHeight > 0) {
                Rect bounds = drawable.copyBounds();
                if (mDrawableWidth > 0) {
                    bounds.right = bounds.left + mDrawableWidth;
                }
                if (mDrawableHeight > 0) {
                    bounds.bottom = bounds.top + mDrawableHeight;
                }
                drawable.setBounds(bounds);
            }
        }
    }
}
