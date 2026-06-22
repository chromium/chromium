// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Custom view for the bottom bar. */
@NullMarked
public class BottomBarView extends LinearLayout {
    private BottomBarButtonContainer mHomeContainer;
    private BottomBarButtonContainer mExtraContainer;
    private BottomBarButtonContainer mNewTabContainer;
    private BottomBarButtonContainer mTabSwitcherContainer;
    private BottomBarButtonContainer mAppMenuContainer;
    private View mNewTabButton;
    private Drawable mNewTabBackground;
    private RippleDrawable mNewTabRippleBackground;
    private RippleDrawable mNewTabRippleNoBackground;
    private BottomBarButtonContainer[] mOtherContainers;
    private RippleDrawable[] mOtherRipples;
    private @Nullable Integer mColorScheme;
    private @Nullable Boolean mNewTabBackgroundVisible;
    private int mNewTabPaddingStart;
    private int mNewTabPaddingTop;
    private int mNewTabPaddingEnd;
    private int mNewTabPaddingBottom;

    public BottomBarView(Context context, @Nullable AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHomeContainer = findViewById(R.id.home_button_container);
        mExtraContainer = findViewById(R.id.extra_button_container);

        mNewTabButton = findViewById(R.id.new_tab_button);
        mNewTabContainer = (BottomBarButtonContainer) mNewTabButton.getParent();

        View tabSwitcherButton = findViewById(R.id.tab_switcher_button);
        mTabSwitcherContainer = (BottomBarButtonContainer) tabSwitcherButton.getParent();

        mAppMenuContainer = findViewById(R.id.app_menu_button_container);

        Context context = getContext();
        mNewTabBackground =
                assertNonNull(context.getDrawable(R.drawable.bottom_bar_new_tab_background))
                        .mutate();

        mNewTabRippleBackground =
                new RippleDrawable(ColorStateList.valueOf(0), mNewTabBackground, mNewTabBackground);

        mNewTabRippleNoBackground =
                BottomBarUtils.createHoverableRipple(context, BrandedColorScheme.APP_DEFAULT);

        mOtherContainers =
                new BottomBarButtonContainer[] {
                    mHomeContainer, mExtraContainer, mTabSwitcherContainer, mAppMenuContainer
                };
        mOtherRipples = new RippleDrawable[mOtherContainers.length];

        mNewTabPaddingStart = mNewTabButton.getPaddingStart();
        mNewTabPaddingTop = mNewTabButton.getPaddingTop();
        mNewTabPaddingEnd = mNewTabButton.getPaddingEnd();
        mNewTabPaddingBottom = mNewTabButton.getPaddingBottom();
    }

    void setColorScheme(@BrandedColorScheme int colorScheme) {
        Context context = getContext();
        setBackgroundColor(BottomBarUtils.getBottomBarBackgroundColor(context, colorScheme));
        mNewTabBackground.setTint(BottomBarUtils.getColorSurfaceBright(context, colorScheme));

        @ColorInt
        int rippleColorBackground = BottomBarUtils.getRippleColorBackground(context, colorScheme);
        mNewTabRippleBackground.setColor(ColorStateList.valueOf(rippleColorBackground));

        @ColorInt
        int rippleColorNoBackground =
                BottomBarUtils.getRippleColorNoBackground(context, colorScheme);

        if (mColorScheme == null || mColorScheme != colorScheme) {
            mColorScheme = colorScheme;
            for (int i = 0; i < mOtherContainers.length; i++) {
                mOtherRipples[i] = BottomBarUtils.createHoverableRipple(context, colorScheme);
                mOtherContainers[i].setTargetBackground(mOtherRipples[i]);
            }
            mNewTabRippleNoBackground = BottomBarUtils.createHoverableRipple(context, colorScheme);
            if (mNewTabBackgroundVisible == null || !mNewTabBackgroundVisible) {
                setNewTabButtonBackground(mNewTabRippleNoBackground);
            }
        }

        ColorStateList noBackgroundTint = ColorStateList.valueOf(rippleColorNoBackground);
        for (RippleDrawable ripple : mOtherRipples) {
            if (ripple != null) {
                ripple.setColor(noBackgroundTint);
            }
        }
        mNewTabRippleNoBackground.setColor(noBackgroundTint);

        ColorStateList tint = BottomBarUtils.getIconColorStateList(context, colorScheme);

        mHomeContainer.setColorScheme(tint, colorScheme);
        mExtraContainer.setColorScheme(tint, colorScheme);
        mNewTabContainer.setColorScheme(tint, colorScheme);
        mTabSwitcherContainer.setColorScheme(tint, colorScheme);
        mAppMenuContainer.setColorScheme(tint, colorScheme);
    }

    /*package*/ void setNewTabBackgroundVisible(boolean visible) {
        if (mNewTabBackgroundVisible != null && mNewTabBackgroundVisible == visible) {
            return;
        }
        mNewTabBackgroundVisible = visible;
        if (visible) {
            setNewTabButtonBackground(mNewTabRippleBackground);
        } else {
            setNewTabButtonBackground(mNewTabRippleNoBackground);
        }
    }

    private void setNewTabButtonBackground(Drawable drawable) {
        if (mNewTabButton.getBackground() == drawable) {
            return;
        }
        mNewTabButton.setBackground(drawable);
        mNewTabButton.setPaddingRelative(
                mNewTabPaddingStart, mNewTabPaddingTop, mNewTabPaddingEnd, mNewTabPaddingBottom);
    }

    /**
     * Sets the visibility of the button container associated with the given action ID.
     *
     * @param actionId The ID of the action.
     * @param visible True to make visible, false to make GONE.
     */
    public void setButtonVisibility(int actionId, boolean visible) {
        BottomBarButtonContainer container = getContainerForAction(actionId);
        if (container != null) {
            container.setVisibility(visible ? View.VISIBLE : View.GONE);
        }
    }

    @Nullable BottomBarButtonContainer getContainerForAction(@ActionId int actionId) {
        switch (actionId) {
            case ActionId.HOME_BUTTON:
                return mHomeContainer;
            case ActionId.GLIC:
                return mExtraContainer;
            case ActionId.NEW_TAB:
                return mNewTabContainer;
            case ActionId.TAB_SWITCHER:
                return mTabSwitcherContainer;
            case ActionId.APP_MENU:
                return mAppMenuContainer;
            default:
                return null;
        }
    }
}
