// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;

import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.listmenu.ListMenuButton;

/**
 * The Button used for switching tabs. Currently this class is only being used for the bottom
 * toolbar tab switcher button.
 */
public class TabSwitcherButtonView extends ListMenuButton {
    /** A drawable for the tab switcher icon. */
    private TabSwitcherDrawable mTabSwitcherButtonDrawable;

    public TabSwitcherButtonView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTabSwitcherButtonDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        getContext(),
                        BrandedColorScheme.APP_DEFAULT,
                        TabSwitcherDrawableLocation.TAB_SWITCHER_TOOLBAR);
        setImageDrawable(mTabSwitcherButtonDrawable);
    }

    /**
     * @param numberOfTabs The number of open tabs.
     */
    public void updateTabCountVisuals(int numberOfTabs) {
        mTabSwitcherButtonDrawable.updateForTabCount(numberOfTabs, false);
    }

    /**
     * @param tint The {@ColorStateList} used to tint the button.
     */
    public void setTint(ColorStateList tint) {
        mTabSwitcherButtonDrawable.setTint(tint);
    }
}
