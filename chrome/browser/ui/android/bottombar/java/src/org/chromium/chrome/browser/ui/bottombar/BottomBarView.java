// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.ui.util.ColorUtils;

/** Custom view for the bottom bar. */
@NullMarked
public class BottomBarView extends LinearLayout {
    private View mHomeContainer;
    private View mNewTabButton;
    private Drawable mNewTabBackground;
    private RippleDrawable mNewTabRippleBackground;
    private RippleDrawable mNewTabRippleNoBackground;

    public BottomBarView(Context context, @Nullable AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHomeContainer = findViewById(R.id.home_button_container);
        mNewTabButton = findViewById(R.id.new_tab_button);

        mNewTabBackground =
                AppCompatResources.getDrawable(
                                getContext(), R.drawable.bottom_bar_new_tab_background)
                        .mutate();

        mNewTabRippleBackground =
                new RippleDrawable(ColorStateList.valueOf(0), mNewTabBackground, mNewTabBackground);

        mNewTabRippleNoBackground = new RippleDrawable(ColorStateList.valueOf(0), null, null);
        mNewTabRippleNoBackground.setRadius(
                getResources().getDimensionPixelSize(R.dimen.bottom_bar_ripple_radius));
    }

    void setColorScheme(@BrandedColorScheme int colorScheme) {
        setBackgroundColor(BottomBarUtils.getBottomBarBackgroundColor(getContext(), colorScheme));
        mNewTabBackground.setTint(BottomBarUtils.getColorSurfaceBright(getContext(), colorScheme));

        boolean isIncognito = colorScheme == BrandedColorScheme.INCOGNITO;
        @ColorInt int onSurface = IncognitoColors.getColorOnSurface(getContext(), isIncognito);

        @ColorInt
        int rippleColorBackground = ColorUtils.setAlphaComponentWithFloat(onSurface, 0.08f);
        mNewTabRippleBackground.setColor(ColorStateList.valueOf(rippleColorBackground));

        @ColorInt
        int rippleColorNoBackground = ColorUtils.setAlphaComponentWithFloat(onSurface, 0.10f);
        mNewTabRippleNoBackground.setColor(ColorStateList.valueOf(rippleColorNoBackground));
    }

    void setHomeButtonVisible(boolean visible) {
        mHomeContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setNewTabBackgroundVisible(boolean visible) {
        if (visible) {
            mNewTabButton.setBackground(mNewTabRippleBackground);
        } else {
            mNewTabButton.setBackground(mNewTabRippleNoBackground);
        }
    }
}
