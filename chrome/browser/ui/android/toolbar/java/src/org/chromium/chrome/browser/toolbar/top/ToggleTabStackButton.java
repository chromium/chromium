// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;

import androidx.annotation.Nullable;
import androidx.annotation.PluralsRes;
import androidx.appcompat.widget.TooltipCompat;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.listmenu.ListMenuButton;

/**
 * A button displaying the number of open tabs. Clicking the button toggles the tab switcher view.
 * TODO(twellington): Replace with TabSwitcherButtonCoordinator so code can be shared with bottom
 * toolbar.
 */
public class ToggleTabStackButton extends ListMenuButton implements TabSwitcherDrawable.Observer {
    private final Callback<Integer> mTabCountSupplierObserver = this::onUpdateTabCount;
    private final Callback<TabModelDotInfo> mNotificationDotObserver =
            this::onUpdateNotificationDot;
    private TabSwitcherDrawable mTabSwitcherButtonDrawable;
    private ObservableSupplier<Integer> mTabCountSupplier;
    private ObservableSupplier<TabModelDotInfo> mNotificationDotSupplier;
    private Supplier<Boolean> mIsIncognitoSupplier;
    private @Nullable UserEducationHelper mUserEducationHelper;

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
        if (mTabCountSupplier != null) {
            mTabCountSupplier.removeObserver(mTabCountSupplierObserver);
        }
        if (mNotificationDotSupplier != null) {
            mNotificationDotSupplier.removeObserver(mNotificationDotObserver);
        }
        mTabSwitcherButtonDrawable.removeTabSwitcherDrawableObserver(this);
    }

    void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        mTabSwitcherButtonDrawable.setTint(
                ThemeUtils.getThemedToolbarIconTint(getContext(), brandedColorScheme));
        mTabSwitcherButtonDrawable.setNotificationBackground(brandedColorScheme);
        if (mIsIncognitoSupplier != null) {
            mTabSwitcherButtonDrawable.setIncognitoStatus(mIsIncognitoSupplier.get());
        }
    }

    /**
     * @param tabCountSupplier A supplier used to observe the number of tabs in the current model.
     * @param notificationDotSupplier A supplier used to observe whether to show the notification
     *     dot.
     * @param isIncognitoSupplier A supplier used to check for incongito state.
     * @param userEducationHelper Used to show an IPH.
     */
    void setSuppliers(
            ObservableSupplier<Integer> tabCountSupplier,
            ObservableSupplier<TabModelDotInfo> notificationDotSupplier,
            Supplier<Boolean> isIncognitoSupplier,
            UserEducationHelper userEducationHelper) {
        assert mTabCountSupplier == null : "setSuppliers should only be called once.";

        mTabCountSupplier = tabCountSupplier;
        tabCountSupplier.addObserver(mTabCountSupplierObserver);

        mNotificationDotSupplier = notificationDotSupplier;
        notificationDotSupplier.addObserver(mNotificationDotObserver);

        mIsIncognitoSupplier = isIncognitoSupplier;
        mUserEducationHelper = userEducationHelper;
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
     * @param alpha Integer (0-255) alpha level to draw at.
     */
    public void drawTabSwitcherAnimationOverlay(Canvas canvas, int alpha) {
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

    private void onUpdateTabCount(int tabCount) {
        setEnabled(tabCount >= 1);
        mTabSwitcherButtonDrawable.updateForTabCount(tabCount, mIsIncognitoSupplier.get());
    }

    private void onUpdateNotificationDot(TabModelDotInfo tabModelDotInfo) {
        mTabSwitcherButtonDrawable.setNotificationIconStatus(tabModelDotInfo.showDot);
        if (tabModelDotInfo.showDot && mUserEducationHelper != null) {
            String tabGroupTitle = tabModelDotInfo.tabGroupTitle;
            String contentString =
                    getResources().getString(R.string.tab_group_update_iph_text, tabGroupTitle);
            IphCommand iphCommand =
                    new IphCommandBuilder(
                                    getResources(),
                                    FeatureConstants.TAB_GROUP_SHARE_UPDATE_FEATURE,
                                    contentString,
                                    contentString)
                            .setAnchorView(this)
                            .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                            .build();
            mUserEducationHelper.requestShowIph(iphCommand);
        }
    }

    /** Returns whether the button should show a notification icon. */
    public boolean shouldShowNotificationIcon() {
        return mTabSwitcherButtonDrawable.getShowIconNotificationStatus();
    }
}
