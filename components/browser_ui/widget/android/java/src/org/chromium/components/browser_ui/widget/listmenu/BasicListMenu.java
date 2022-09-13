// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.listmenu;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
import java.util.List;

/**
 * An implementation of a list menu. Uses app_menu_layout as the default layout of menu and
 * list_menu_item as the default layout of a menu item.
 */
public class BasicListMenu implements ListMenu, OnItemClickListener {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListMenuItemType.DIVIDER, ListMenuItemType.MENU_ITEM})
    public static @interface ListMenuItemType {
        int DIVIDER = 0;
        int MENU_ITEM = 1;
    }

    /**
     * Helper function to build a list menu item. Pass 0 for attributes that aren't
     * applicable to the menu item (e.g. if there is no icon or text).
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item.
     * @param enabled Whether or not this menu item should be enabled.
     * @return ListItem Representing an item with text or icon.
     */
    public static ListItem buildMenuListItem(@StringRes int titleId, @IdRes int menuId,
            @DrawableRes int startIconId, boolean enabled) {
        return new ListItem(ListMenuItemType.MENU_ITEM,
                buildPropertyModel(titleId, menuId, startIconId, enabled));
    }

    /**
     * Helper function to build a list menu item. Pass 0 for attributes that aren't
     * applicable to the menu item (e.g. if there is no icon or text).
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param endIconId The icon on the end of the menu item.
     * @param enabled Whether or not this menu item should be enabled.
     * @return ListItem Representing an item with text or icon.
     */
    public static ListItem buildMenuListItemWithEndIcon(@StringRes int titleId, @IdRes int menuId,
            @DrawableRes int endIconId, boolean enabled) {
        return new ListItem(ListMenuItemType.MENU_ITEM,
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE_ID, titleId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, menuId)
                        .with(ListMenuItemProperties.END_ICON_ID, endIconId)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.TINT_COLOR_ID,
                                R.color.default_icon_color_secondary_tint_list)
                        .build());
    }

    /**
     * Helper function to build a list menu item. Set 0 if there is no icon or text.
     * This ListItem is set enabled as default.
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item.
     * @return ListItem Representing an item with text or icon.
     */
    public static ListItem buildMenuListItem(
            @StringRes int titleId, @IdRes int menuId, @DrawableRes int startIconId) {
        return new ListItem(ListMenuItemType.MENU_ITEM,
                buildPropertyModel(titleId, menuId, startIconId, true /* enabled */));
    }

    /**
     * Helper function to build a ListItem of a divider.
     * @return ListItem Representing a divider.
     */
    public static ListItem buildMenuDivider() {
        return new ListItem(ListMenuItemType.DIVIDER, new PropertyModel());
    }

    private final ListView mListView;
    private final Context mContext;
    private final ModelListAdapter mAdapter;
    private final View mContentView;
    private final List<Runnable> mClickRunnables;
    private final Delegate mDelegate;

    /**
     * @param context The {@link Context} to inflate the layout.
     * @param data Data representing the list items.
     *             All items in data are assumed to be enabled.
     * @param delegate The {@link Delegate} that would be called when the menu is clicked.
     */
    public BasicListMenu(@NonNull Context context, ModelList data, Delegate delegate) {
        mContext = context;
        mAdapter = new ListMenuItemAdapter(data);
        mContentView = LayoutInflater.from(context).inflate(R.layout.app_menu_layout, null);
        mListView = mContentView.findViewById(R.id.app_menu_list);
        mListView.setAdapter(mAdapter);
        mListView.setDivider(null);
        mListView.setOnItemClickListener(this);
        mDelegate = delegate;
        mClickRunnables = new LinkedList<>();
        registerListItemTypes();
    }

    /**
     * @param context The {@link Context} to inflate the layout.
     * @param data Data representing the list items.
     *             All items in data are assumed to be enabled.
     * @param delegate The {@link Delegate} that would be called when the menu is clicked.
     * @param backgroundTintColor The background tint color of the menu.
     */
    public BasicListMenu(@NonNull Context context, ModelList data, Delegate delegate,
            @ColorRes int backgroundTintColor) {
        this(context, data, delegate);
        ViewCompat.setBackgroundTintList(mContentView,
                ColorStateList.valueOf(ContextCompat.getColor(mContext, backgroundTintColor)));
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    public ListView getListView() {
        return mListView;
    }

    @Override
    public void addContentViewClickRunnable(Runnable runnable) {
        mClickRunnables.add(runnable);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mDelegate.onItemSelected(((ListItem) mAdapter.getItem(position)).model);
        for (Runnable r : mClickRunnables) {
            r.run();
        }
    }

    @Override
    public int getMaxItemWidth() {
        return UiUtils.computeMaxWidthOfListAdapterItems(mAdapter, mListView);
    }

    private void registerListItemTypes() {
        // clang-format off
        mAdapter.registerType(ListMenuItemType.MENU_ITEM,
            new LayoutViewBuilder(R.layout.list_menu_item),
            ListMenuItemViewBinder::binder);
        mAdapter.registerType(ListMenuItemType.DIVIDER,
            new LayoutViewBuilder(R.layout.app_menu_divider),
            (m, v, p) -> {});
        // clang-format on
    }

    // Internal helper function to build a property model of list menu item.
    private static PropertyModel buildPropertyModel(
            @StringRes int titleId, @IdRes int menuId, @DrawableRes int iconId, boolean enabled) {
        return new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                .with(ListMenuItemProperties.TITLE_ID, titleId)
                .with(ListMenuItemProperties.MENU_ITEM_ID, menuId)
                .with(ListMenuItemProperties.START_ICON_ID, iconId)
                .with(ListMenuItemProperties.ENABLED, enabled)
                .with(ListMenuItemProperties.TINT_COLOR_ID,
                        R.color.default_icon_color_secondary_tint_list)
                .build();
    }
}
