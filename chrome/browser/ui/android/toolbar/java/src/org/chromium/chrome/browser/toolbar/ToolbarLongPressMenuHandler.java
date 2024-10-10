// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.View.OnLongClickListener;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The handler for the toolbar long press menu. */
public class ToolbarLongPressMenuHandler {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MenuItemType.MOVE_ADDRESS_BAR_TO, MenuItemType.COPY_LINK})
    public @interface MenuItemType {
        int MOVE_ADDRESS_BAR_TO = 0;
        int COPY_LINK = 1;
    }

    private AnchoredPopupWindow mPopupMenu;
    private int mPopupMenuMaxWidth;
    @NonNull private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    @Nullable private final OnLongClickListener mOnLongClickListener;
    @NonNull private final SharedPreferencesManager mSharedPreferencesManager;

    /**
     * Creates a new {@link ToolbarLongPressMenuHandler}.
     *
     * @param context current context
     */
    public ToolbarLongPressMenuHandler(
            Context context,
            boolean isCustomTab,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;

        if (ToolbarPositionController.isToolbarPositionCustomizationEnabled(context, isCustomTab)) {
            mOnLongClickListener =
                    (view) -> {
                        if (mOmniboxFocusStateSupplier.get()) {
                            // Do nothing if the URL bar has focus during a long press.
                            return false;
                        }

                        displayMenu(view);
                        return true;
                    };
        } else {
            mOnLongClickListener = null;
        }

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();

        TypedArray a = context.obtainStyledAttributes(null, R.styleable.ListMenuButton);
        mPopupMenuMaxWidth =
                a.getDimensionPixelSize(
                        R.styleable.ListMenuButton_menuMaxWidth,
                        context.getResources().getDimensionPixelSize(R.dimen.list_menu_width));
    }

    /**
     * Return a long-click listener which shows the toolbar popup menu. Return null if toolbar is in
     * CCT or widgets.
     *
     * @return A long-click listener showing the menu.
     */
    protected @Nullable OnLongClickListener getOnLongClickListener() {
        return mOnLongClickListener;
    }

    private void displayMenu(View view) {
        ViewRectProvider provider = new ViewRectProvider(view);

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        view.getContext(),
                        buildMenuItems(),
                        (model) -> {
                            handleMenuClick(model.get(ListMenuItemProperties.MENU_ITEM_ID));
                            mPopupMenu.dismiss();
                        });
        View contentView = listMenu.getContentView();

        mPopupMenu =
                new AnchoredPopupWindow(
                        view.getContext(),
                        view,
                        new ColorDrawable(Color.TRANSPARENT),
                        contentView,
                        provider);

        mPopupMenu.setMaxWidth(mPopupMenuMaxWidth);
        mPopupMenu.setFocusable(true);
        mPopupMenu.setOutsideTouchable(true);
        mPopupMenu.show();
    }

    @VisibleForTesting
    ModelList buildMenuItems() {
        boolean onTop =
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);
        ModelList itemList = new ModelList();
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        onTop
                                ? R.string.toolbar_move_to_the_bottom
                                : R.string.toolbar_move_to_the_top,
                        MenuItemType.MOVE_ADDRESS_BAR_TO,
                        /* iconId= */ 0));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.toolbar_copy_link, MenuItemType.COPY_LINK, /* iconId= */ 0));
        return itemList;
    }

    private void handleMenuClick(int id) {
        // TODO(gangwu): implement to handle the click.
        if (id == MenuItemType.MOVE_ADDRESS_BAR_TO) {
            return;
        } else if (id == MenuItemType.COPY_LINK) {
            return;
        }
    }

    public AnchoredPopupWindow getPopupWindowForTesting() {
        return mPopupMenu;
    }
}
