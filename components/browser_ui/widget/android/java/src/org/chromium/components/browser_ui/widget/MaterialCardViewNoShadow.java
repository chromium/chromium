// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * Extension of {@link FrameLayout} that sets background resource to a rounded corner rectangle,
 * with dynamic background color from ElevationOverlayProvider based on card elevation.
 * Reuse the name of MaterialCardViewNoShadow to keep the same usage.
 * But this class is no longer an extension of MaterialCardView.
 */
public class MaterialCardViewNoShadow extends FrameLayout {
    public MaterialCardViewNoShadow(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public MaterialCardViewNoShadow(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        final TypedArray typedArray =
                context.obtainStyledAttributes(
                        attrs,
                        R.styleable.MaterialCardViewNoShadow,
                        defStyleAttr,
                        R.style.MaterialCardStyle);
        final float cornerSize =
                typedArray.getDimensionPixelSize(
                        R.styleable.MaterialCardViewNoShadow_cornerRadius, 0);
        final float elevation =
                typedArray.getDimensionPixelSize(
                        R.styleable.MaterialCardViewNoShadow_cardElevation, 0);
        typedArray.recycle();

        setBackgroundResource(R.drawable.card_with_corners_background);
        GradientDrawable gradientDrawable = (GradientDrawable) getBackground();
        gradientDrawable.setCornerRadius(cornerSize);
        gradientDrawable.setColor(ChromeColors.getSurfaceColor(getContext(), elevation));
    }
}
