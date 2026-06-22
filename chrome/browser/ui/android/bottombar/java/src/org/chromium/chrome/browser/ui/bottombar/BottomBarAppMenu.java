// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.ui.actions.appmenu.MenuButtonState;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** A customized FrameLayout used exclusively for the Bottom Bar App Menu button. */
@NullMarked
public class BottomBarAppMenu extends FrameLayout implements TintObserver {
    private @Nullable ImageButton mMenuImageButton;
    private @Nullable ImageView mBottomUpdateBadgeView;
    private @Nullable MenuButtonState mMenuButtonState;
    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The Context the view is running in.
     * @param attrs The attributes of the XML tag that is inflating the view.
     */
    public BottomBarAppMenu(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuImageButton = findViewById(R.id.menu_button);
        mBottomUpdateBadgeView = findViewById(R.id.menu_badge);
    }

    @Override
    public CharSequence getAccessibilityClassName() {
        return Button.class.getName();
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mBrandedColorScheme = brandedColorScheme;
        if (mMenuImageButton != null && tint != null) {
            mMenuImageButton.setImageTintList(tint);
        }
        updateImageResources();
    }

    /**
     * Returns the inner menu image button.
     *
     * @return The {@link ImageButton} representing the menu icon.
     */
    public @Nullable ImageButton getImageButton() {
        return mMenuImageButton;
    }

    /**
     * Sets the state of the update badge.
     *
     * @param buttonState The {@link MenuButtonState} representing the current badge state.
     */
    public void setBadgeUpdateState(@Nullable MenuButtonState buttonState) {
        mMenuButtonState = buttonState;
        updateImageResources();
    }

    /**
     * Sets whether the update badge should be visible.
     *
     * @param visible True if the badge should be visible, false otherwise.
     */
    public void setAppMenuUpdateBadgeVisible(boolean visible) {
        if (mBottomUpdateBadgeView == null) return;
        mBottomUpdateBadgeView.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    private void updateImageResources() {
        if (mBottomUpdateBadgeView == null) return;
        if (mMenuButtonState == null) {
            mBottomUpdateBadgeView.setImageDrawable(null);
            return;
        }
        @DrawableRes int drawable = getUpdateBadgeIcon(mMenuButtonState, mBrandedColorScheme);
        if (drawable != Resources.ID_NULL) {
            mBottomUpdateBadgeView.setImageDrawable(
                    AppCompatResources.getDrawable(getContext(), drawable));
        } else {
            mBottomUpdateBadgeView.setImageDrawable(null);
        }
    }

    private @DrawableRes int getUpdateBadgeIcon(
            MenuButtonState buttonState, @BrandedColorScheme int brandedColorScheme) {
        @DrawableRes int drawable = buttonState.adaptiveBadgeIcon;
        if (brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME
                || brandedColorScheme == BrandedColorScheme.INCOGNITO) {
            drawable = buttonState.lightBadgeIcon;
        } else if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME) {
            drawable = buttonState.darkBadgeIcon;
        }
        return drawable;
    }
}
