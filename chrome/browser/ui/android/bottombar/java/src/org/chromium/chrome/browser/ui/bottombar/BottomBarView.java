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
import org.chromium.ui.util.ValueUtils;

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
    private float mDisabledAlpha;
    private @ColorInt int mNewTabBackgroundTintForTesting;
    private @ColorInt int mNewTabRippleBackgroundColorForTesting;
    private @ColorInt int mOtherRipplesColorForTesting;
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
        mDisabledAlpha =
                ValueUtils.getFloat(context.getResources(), R.dimen.default_disabled_alpha);
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

    /**
     * Sets the bright surface background tint for the new tab button.
     *
     * @param color The bright surface color int.
     */
    public void setNewTabBackgroundTint(@ColorInt int color) {
        mNewTabBackgroundTintForTesting = color;
        mNewTabBackground.setTint(color);
        mNewTabButton.invalidate();
    }

    /**
     * Sets the ripple color for the new tab button with a background.
     *
     * @param color The ripple color int.
     */
    public void setNewTabRippleBackgroundColor(@ColorInt int color) {
        mNewTabRippleBackgroundColorForTesting = color;
        mNewTabRippleBackground.setColor(ColorStateList.valueOf(color));
    }

    /**
     * Sets the ripple color for buttons without a background.
     *
     * @param color The ripple color int.
     */
    public void setOtherRipplesColor(@ColorInt int color) {
        mOtherRipplesColorForTesting = color;
        ColorStateList noBackgroundTint = ColorStateList.valueOf(color);
        for (RippleDrawable ripple : mOtherRipples) {
            if (ripple != null) {
                ripple.setColor(noBackgroundTint);
            }
        }
        mNewTabRippleNoBackground.setColor(noBackgroundTint);
    }

    /**
     * Sets the icon tint across all button containers derived from an onSurface color.
     *
     * @param onSurfaceColor The onSurface color int.
     */
    public void setIconOnSurfaceColor(@ColorInt int onSurfaceColor) {
        ColorStateList tint =
                BottomBarUtils.getIconColorStateListFromOnSurface(onSurfaceColor, mDisabledAlpha);
        @BrandedColorScheme
        int scheme = mColorScheme != null ? mColorScheme : BrandedColorScheme.APP_DEFAULT;
        mHomeContainer.setColorScheme(tint, scheme);
        mExtraContainer.setColorScheme(tint, scheme);
        mNewTabContainer.setColorScheme(tint, scheme);
        mTabSwitcherContainer.setColorScheme(tint, scheme);
        mAppMenuContainer.setColorScheme(tint, scheme);
    }

    /** Resets the view colors to its configured {@link BrandedColorScheme}. */
    public void resetColors() {
        @BrandedColorScheme
        int colorScheme = mColorScheme != null ? mColorScheme : BrandedColorScheme.APP_DEFAULT;
        mColorScheme = null;
        setColorScheme(colorScheme);
    }

    public @ColorInt int getNewTabBackgroundTintForTesting() {
        return mNewTabBackgroundTintForTesting;
    }

    public @ColorInt int getNewTabRippleBackgroundColorForTesting() {
        return mNewTabRippleBackgroundColorForTesting;
    }

    public @ColorInt int getOtherRipplesColorForTesting() {
        return mOtherRipplesColorForTesting;
    }

    Drawable getNewTabBackgroundForTesting() {
        return mNewTabBackground;
    }

    RippleDrawable getNewTabRippleBackgroundForTesting() {
        return mNewTabRippleBackground;
    }

    RippleDrawable getNewTabRippleNoBackgroundForTesting() {
        return mNewTabRippleNoBackground;
    }

    RippleDrawable[] getOtherRipplesForTesting() {
        return mOtherRipples;
    }

    void setColorScheme(@BrandedColorScheme int colorScheme) {
        Context context = getContext();
        setBackgroundColor(BottomBarUtils.getBottomBarBackgroundColor(context, colorScheme));
        setNewTabBackgroundTint(BottomBarUtils.getColorSurfaceBright(context, colorScheme));
        setNewTabRippleBackgroundColor(
                BottomBarUtils.getRippleColorBackground(context, colorScheme));
        setOtherRipplesColor(BottomBarUtils.getRippleColorNoBackground(context, colorScheme));

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

        ColorStateList tint = BottomBarUtils.getIconColorStateList(context, colorScheme);

        mHomeContainer.setColorScheme(tint, colorScheme);
        mExtraContainer.setColorScheme(tint, colorScheme);
        mNewTabContainer.setColorScheme(tint, colorScheme);
        mTabSwitcherContainer.setColorScheme(tint, colorScheme);
        mAppMenuContainer.setColorScheme(tint, colorScheme);
    }

    public void setNewTabBackgroundVisible(boolean visible) {
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
            case ActionId.AI_MODE:
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

    /**
     * Inflates all ViewStubs in the bottom bar for testing purposes. This respects configuration
     * flags, so it will not inflate buttons that should not be present in the current configuration
     * (e.g., Home or App Menu).
     */
    public void inflateAllStubsForTesting() {
        if (BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled()) {
            mHomeContainer.inflateStub();
        }
        mExtraContainer.inflateStub();
        mNewTabContainer.inflateStub();
        mTabSwitcherContainer.inflateStub();
        if (BottomBarConfigUtils.shouldIncludeAppMenuButton()) {
            if (!mAppMenuContainer.hasTargetView()) {
                if (BottomBarConfigUtils.shouldShowAppMenuUpdateBadge()) {
                    mAppMenuContainer.setStubLayoutResource(R.layout.bottom_bar_app_menu_template);
                }
                mAppMenuContainer.inflateStub();
            }
        }
    }
}
