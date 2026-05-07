// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.bars_common;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.android.bars_common.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.listmenu.ListMenuButton;

/** A tab switcher button view. Designed to be used with the Actions framework. */
@NullMarked
public class TabSwitcherButtonView extends ListMenuButton {
    private TabSwitcherDrawable mTabSwitcherDrawable;

    public TabSwitcherButtonView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTabSwitcherDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        getContext(),
                        BrandedColorScheme.APP_DEFAULT,
                        TabSwitcherDrawableLocation.TAB_TOOLBAR);
        setImageDrawable(mTabSwitcherDrawable);
    }

    @Override
    public void setImageTintList(@Nullable ColorStateList tint) {
        if (mTabSwitcherDrawable != null && tint != null) {
            mTabSwitcherDrawable.setTintList(tint);
        }
        super.setImageTintList(tint);
    }

    public void setTabCount(int tabCount, boolean isIncognito) {
        mTabSwitcherDrawable.updateForTabCount(tabCount, isIncognito);
    }

    public void setNotificationDotVisible(boolean showDot) {
        mTabSwitcherDrawable.setNotificationIconStatus(showDot);
    }

    public void endRippleAnimation() {
        if (getBackground() != null) {
            getBackground().jumpToCurrentState();
        }
    }

    public void setDrawableForTesting(TabSwitcherDrawable drawable) {
        mTabSwitcherDrawable = drawable;
        setImageDrawable(drawable);
    }

    public boolean isNotificationDotVisible() {
        return mTabSwitcherDrawable.getShowIconNotificationStatus();
    }
}
