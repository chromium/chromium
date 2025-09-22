// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;

/**
 * A LinearLayout that can be contained in a setting page. It will be styled by the {@link
 * org.chromium.components.browser_ui.settings.SettingsStylingController}.
 */
@NullMarked
public class ContainedLinearLayout extends LinearLayout implements CustomStyledContainer {
    private @BackgroundStyle int mBackgroundStyle = BackgroundStyle.STANDARD;
    private @ColorInt int mCustomBackgroundColor = CustomStyledContainer.DEFAULT_COLOR;

    /** Constructor for inflating from XML. */
    public ContainedLinearLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the background style for this view.
     *
     * @param style The {@link BackgroundStyle} to use.
     */
    public void setBackgroundStyle(@BackgroundStyle int style) {
        mBackgroundStyle = style;
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        return mBackgroundStyle;
    }

    /**
     * Sets the custom background color for this view.
     *
     * @param color The color to use.
     */
    public void setCustomBackgroundColor(@ColorInt int color) {
        mCustomBackgroundColor = color;
    }

    @Override
    public int getCustomBackgroundColor() {
        return mCustomBackgroundColor;
    }
}
