// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.widget.RectProvider;

/**
 * Root component for the {@link HomeButton} on the toolbar. Currently owns context menu for the
 * home button.
 */
// TODO(crbug.com/40676825): Fix the visibility bug on NTP.
@NullMarked
public class HomeButtonCoordinator {
    private static final int ID_SETTINGS = 0;

    private final Context mContext;
    private final HomeButton mHomeButton;
    private final Supplier<Boolean> mIsHomeButtonMenuDisabled;

    private final Callback<Context> mOnMenuClickCallback;
    private MVCListAdapter.@Nullable ModelList mMenuList;
    private @Nullable ListMenuDelegate mListMenuDelegate;

    /**
     * @param context The Android context used for various view operations.
     * @param homeButton The concrete {@link View} class for this MVC component.
     * @param onClickListener Listener invoked when button is clicked.
     * @param onMenuClickCallback Callback when home button menu item is clicked.
     * @param isHomepageMenuDisabledSupplier Supplier for whether the home button menu is enabled.
     */
    public HomeButtonCoordinator(
            Context context,
            View homeButton,
            OnClickListener onClickListener,
            Callback<Context> onMenuClickCallback,
            Supplier<Boolean> isHomepageMenuDisabledSupplier) {
        mContext = context;
        mHomeButton = (HomeButton) homeButton;
        mOnMenuClickCallback = onMenuClickCallback;
        mIsHomeButtonMenuDisabled = isHomepageMenuDisabledSupplier;
        mHomeButton.setOnLongClickListener(this::onLongClickHomeButton);
        mHomeButton.setOnClickListener(onClickListener);
    }

    @VisibleForTesting
    boolean onLongClickHomeButton(View view) {
        if (view != mHomeButton || mIsHomeButtonMenuDisabled.get()) return false;

        if (mListMenuDelegate == null) {
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
            mListMenuDelegate =
                    new ListMenuDelegate() {
                        @Override
                        public ListMenu getListMenu() {
                            return listMenu;
                        }

                        @Override
                        public RectProvider getRectProvider(View listMenuButton) {
                            return rectProvider;
                        }
                    };
            mHomeButton.setDelegate(mListMenuDelegate, false);
        }
        mHomeButton.showMenu();
        return true;
    }

    public MVCListAdapter.@Nullable ModelList getMenuForTesting() {
        return mMenuList;
    }
}
