// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;

import androidx.annotation.PluralsRes;
import androidx.appcompat.widget.TooltipCompat;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.listmenu.ListMenuButton;

/**
 * A button displaying the number of open tabs. Clicking the button toggles the tab switcher view.
 * TODO(twellington): Replace with TabSwitcherButtonCoordinator so code can be shared with bottom
 * toolbar.
 */
@NullMarked
public class ToggleTabStackButton extends ListMenuButton implements TabSwitcherDrawable.Observer {
    private TabSwitcherDrawable mTabSwitcherButtonDrawable;
    private ObservableSupplier<Integer> mTabCountSupplier;

    public ToggleTabStackButton(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mTabSwitcherButtonDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        getContext(),
                        BrandedColorScheme.APP_DEFAULT,
                        TabSwitcherDrawableLocation.TAB_TOOLBAR);
        setImageDrawable(mTabSwitcherButtonDrawable);
        mTabSwitcherButtonDrawable.addTabSwitcherDrawableObserver(this);
    }

    /** Called to destroy the tab stack button. */
    void destroy() {
        mTabSwitcherButtonDrawable.removeTabSwitcherDrawableObserver(this);
    }

    void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        mTabSwitcherButtonDrawable.setTint(
                ThemeUtils.getThemedToolbarIconTint(getContext(), brandedColorScheme));
        mTabSwitcherButtonDrawable.setNotificationBackground(brandedColorScheme);
    }

    /**
     * @param tabCountSupplier A supplier used to observe the number of tabs in the current model.
     */
    @Initializer
    void setSuppliers(ObservableSupplier<Integer> tabCountSupplier) {
        assert mTabCountSupplier == null : "setSuppliers should only be called once.";

        mTabCountSupplier = tabCountSupplier;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("ToggleTabStackButton.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("ToggleTabStackButton.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    // TabSwitcherDrawable.Observer implementation.

    @Override
    public void onDrawableStateChanged() {
        @PluralsRes
        int drawableDescRes = R.plurals.accessibility_toolbar_btn_tabswitcher_toggle_default;
        // TODO(ritikagup) : Use utility for the check.
        if ((ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                        || ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_JOIN_ONLY))
                && mTabSwitcherButtonDrawable.getShowIconNotificationStatus()) {
            drawableDescRes =
                    R.plurals
                            .accessibility_toolbar_btn_tabswitcher_toggle_default_with_notification;
        }

        int tabCount = mTabCountSupplier.get();
        String drawableText = getResources().getQuantityString(drawableDescRes, tabCount, tabCount);
        setContentDescription(drawableText);
        TooltipCompat.setTooltipText(this, drawableText);
    }

    /**
     * Draws the current visual state of this component for the purposes of rendering the tab
     * switcher animation, setting the alpha to fade the view by the appropriate amount.
     *
     * @param canvas Canvas to draw to.
     */
    public void drawTabSwitcherAnimationOverlay(Canvas canvas) {
        int backgroundWidth = mTabSwitcherButtonDrawable.getIntrinsicWidth();
        int backgroundHeight = mTabSwitcherButtonDrawable.getIntrinsicHeight();
        int backgroundLeft =
                (getWidth() - getPaddingLeft() - getPaddingRight() - backgroundWidth) / 2;
        backgroundLeft += getPaddingLeft();
        int backgroundTop =
                (getHeight() - getPaddingTop() - getPaddingBottom() - backgroundHeight) / 2;
        backgroundTop += getPaddingTop();
        canvas.translate(backgroundLeft, backgroundTop);

        int previousAlpha = mTabSwitcherButtonDrawable.getAlpha();
        mTabSwitcherButtonDrawable.setAlpha(255);
        mTabSwitcherButtonDrawable.draw(canvas);
        // restore alpha.
        getDrawable().setAlpha(previousAlpha);
    }

    public TabSwitcherDrawable getTabSwitcherDrawableForTesting() {
        return mTabSwitcherButtonDrawable;
    }

    void updateTabCount(int tabCount, boolean isIncognito) {
        mTabSwitcherButtonDrawable.updateForTabCount(tabCount, isIncognito);
    }

    void setIncognitoState(boolean incognito) {
        mTabSwitcherButtonDrawable.setIncognitoStatus(incognito);
        var toolbarIconRippleId =
                incognito
                        ? R.drawable.default_icon_background_baseline
                        : R.drawable.default_icon_background;
        setBackgroundResource(toolbarIconRippleId);
    }

    public void onUpdateNotificationDot(TabModelDotInfo tabModelDotInfo) {
        mTabSwitcherButtonDrawable.setNotificationIconStatus(tabModelDotInfo.showDot);
    }

    /** Returns whether the button should show a notification icon. */
    public boolean shouldShowNotificationIcon() {
        return mTabSwitcherButtonDrawable.getShowIconNotificationStatus();
    }
}
