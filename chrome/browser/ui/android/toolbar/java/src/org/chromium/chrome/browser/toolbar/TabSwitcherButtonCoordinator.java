// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.ColorStateList;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The controller for the tab switcher button. This class handles all interactions that the tab
 * switcher button has with the outside world.
 */
public class TabSwitcherButtonCoordinator {
    /**
     *  The model that handles events from outside the tab switcher button. Normally the coordinator
     *  should acces the mediator which then updates the model. Since this component is very simple
     *  the mediator is omitted.
     */
    private final PropertyModel mTabSwitcherButtonModel =
            new PropertyModel(TabSwitcherButtonProperties.ALL_KEYS);

    private final Callback<Integer> mTabCountObserver;

    private ThemeColorProvider mThemeColorProvider;
    private TintObserver mTintObserver;

    private ObservableSupplier<Integer> mTabCountSupplier;

    /**
     * Build the controller that manages the tab switcher button.
     * @param view The {@link TabSwitcherButtonView} the controller manages.
     */
    public TabSwitcherButtonCoordinator(TabSwitcherButtonView view) {
        PropertyModelChangeProcessor.create(
                mTabSwitcherButtonModel, view, new TabSwitcherButtonViewBinder());
        mTabCountObserver =
                (tabCount) -> {
                    mTabSwitcherButtonModel.set(
                            TabSwitcherButtonProperties.NUMBER_OF_TABS, tabCount);
                };
    }

    /**
     * @param onClickListener An {@link OnClickListener} that is triggered when the tab switcher
     *                        button is clicked.
     */
    public void setTabSwitcherListener(OnClickListener onClickListener) {
        mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.ON_CLICK_LISTENER, onClickListener);
    }

    public void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mTintObserver =
                new TintObserver() {
                    @Override
                    public void onTintChanged(
                            ColorStateList tint,
                            ColorStateList activityFocusTint,
                            @BrandedColorScheme int brandedColorScheme) {
                        mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.TINT, tint);
                    }
                };
        mThemeColorProvider.addTintObserver(mTintObserver);
        mTabSwitcherButtonModel.set(
                TabSwitcherButtonProperties.TINT, mThemeColorProvider.getTint());
    }

    public void setTabCountSupplier(ObservableSupplier<Integer> tabCountSupplier) {
        mTabCountSupplier = tabCountSupplier;
        mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.IS_ENABLED, true);
        mTabCountSupplier.addObserver(mTabCountObserver);
    }

    public void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(mTintObserver);
            mThemeColorProvider = null;
        }
        if (mTabCountSupplier != null) {
            mTabCountSupplier.removeObserver(mTabCountObserver);
            mTabCountSupplier = null;
        }
    }
}
