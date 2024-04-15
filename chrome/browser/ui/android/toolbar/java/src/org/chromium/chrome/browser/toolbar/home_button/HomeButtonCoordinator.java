// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.widget.RectProvider;

/**
 * Root component for the {@link HomeButton} on the toolbar. Currently owns context menu for the
 * home button.
 */
// TODO(crbug.com/40676825): Fix the visibility bug on NTP.
public class HomeButtonCoordinator {
    private static final int ID_SETTINGS = 0;

    private final Context mContext;
    private final HomeButton mHomeButton;
    private final Supplier<Boolean> mIsHomeButtonMenuDisabled;

    private final Callback<Context> mOnMenuClickCallback;
    private MVCListAdapter.ModelList mMenuList;
    private @Nullable ListMenuButtonDelegate mListMenuButtonDelegate;

    /**
     * @param context The Android context used for various view operations.
     * @param homeButton The concrete {@link View} class for this MVC component.
     * @param onMenuClickCallback Callback when home button menu item is clicked.
     * @param isHomepageMenuDisabledSupplier Supplier for whether the home button menu is enabled.
     */
    public HomeButtonCoordinator(
            @NonNull Context context,
            @NonNull View homeButton,
            @NonNull Callback<Context> onMenuClickCallback,
            @NonNull Supplier<Boolean> isHomepageMenuDisabledSupplier) {
        mContext = context;
        mHomeButton = (HomeButton) homeButton;
        mOnMenuClickCallback = onMenuClickCallback;
        mIsHomeButtonMenuDisabled = isHomepageMenuDisabledSupplier;
        mHomeButton.setOnLongClickListener(this::onLongClickHomeButton);
    }

    @VisibleForTesting
    boolean onLongClickHomeButton(View view) {
        if (view != mHomeButton || mIsHomeButtonMenuDisabled.get()) return false;

        if (mListMenuButtonDelegate == null) {
            RectProvider rectProvider = MenuBuilderHelper.getRectProvider(mHomeButton);
            mMenuList = new MVCListAdapter.ModelList();
            mMenuList.add(
                    buildMenuListItem(
                            R.string.options_homepage_edit_title,
                            ID_SETTINGS,
                            R.drawable.ic_edit_24dp));
            BasicListMenu listMenu =
                    BrowserUiListMenuUtils.getBasicListMenu(
                            mContext,
                            mMenuList,
                            (model) -> mOnMenuClickCallback.onResult(mContext));
            mListMenuButtonDelegate =
                    new ListMenuButtonDelegate() {
                        @Override
                        public ListMenu getListMenu() {
                            return listMenu;
                        }

                        @Override
                        public RectProvider getRectProvider(View listMenuButton) {
                            return rectProvider;
                        }
                    };
            mHomeButton.setDelegate(mListMenuButtonDelegate, false);
        }
        mHomeButton.showMenu();
        return true;
    }

    public MVCListAdapter.ModelList getMenuForTesting() {
        return mMenuList;
    }
}
