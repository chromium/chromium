// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;

import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Collection of utility methods related to the browser UI list menus. */
public class BrowserUiListMenuUtils {

    /** @return The default icon tint color state list for list menu item icons. */
    @ColorRes
    public static int getDefaultIconTintColorStateListId() {
        return R.color.default_icon_color_secondary_tint_list;
    }

    /** @return The default text appearance style for list menu item text. */
    @StyleRes
    public static int getDefaultTextAppearanceStyle() {
        return R.style.TextAppearance_BrowserUIListMenuItem;
    }

    /**
     * Convenience method for constructing a {@link BasicListMenu} with the preferred content view.
     *
     * @param context The Android context.
     * @param data The data to display in the list.
     * @param delegate Delegate to handle item clicks.
     */
    @NonNull
    public static BasicListMenu getBasicListMenu(
            @NonNull Context context,
            @NonNull MVCListAdapter.ModelList data,
            @NonNull ListMenu.Delegate delegate) {
        return getBasicListMenu(context, data, delegate, 0);
    }

    /**
     * Convenience method for constructing a {@link BasicListMenu} with the preferred content view.
     *
     * @param context The Android context.
     * @param data The data to display in the list.
     * @param delegate Delegate to handle item clicks.
     * @param backgroundTintColor tint for the menu background.
     */
    @NonNull
    public static BasicListMenu getBasicListMenu(
            @NonNull Context context,
            @NonNull MVCListAdapter.ModelList data,
            @NonNull ListMenu.Delegate delegate,
            @ColorRes int backgroundTintColor) {
        View contentView = LayoutInflater.from(context).inflate(R.layout.app_menu_layout, null);
        ListView listView = contentView.findViewById(R.id.app_menu_list);
        return new BasicListMenu(
                context, data, contentView, listView, delegate, backgroundTintColor);
    }

    /**
     * Helper function to build a list menu item. Pass 0 for attributes that aren't applicable to
     * the menu item (e.g. if there is no icon or text).
     *
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item. Pass 0 for no icon.
     * @param enabled Whether or not this menu item should be enabled.
     * @return ListItem Representing an item with text or icon.
     */
    @NonNull
    public static ListItem buildMenuListItem(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            boolean enabled) {
        return new ListItem(
                ListMenuItemType.MENU_ITEM,
                buildPropertyModel(titleId, menuId, startIconId, enabled));
    }

    /**
     * Helper function to build a list menu item. Set 0 if there is no icon or text. This ListItem
     * is set enabled as default.
     *
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item. Pass 0 for no icon.
     * @return ListItem Representing an item with text or icon.
     */
    @NonNull
    public static ListItem buildMenuListItem(
            @StringRes int titleId, @IdRes int menuId, @DrawableRes int startIconId) {
        return new ListItem(
                ListMenuItemType.MENU_ITEM,
                buildPropertyModel(titleId, menuId, startIconId, /* enabled= */ true));
    }

    /**
     * Helper function to build a list menu item. Set 0 if there is no icon or text. This ListItem
     * is set enabled as default.
     *
     * @param title The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item. Pass 0 for no icon.
     * @param enabled Whether or not this menu item should be enabled.
     * @return ListItem Representing an item with text or icon.
     */
    @NonNull
    public static ListItem buildMenuListItem(
            @NonNull String title,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            boolean enabled) {
        return buildMenuListItem(title, menuId, startIconId, null, enabled);
    }

    /**
     * Helper function to build a list menu item. Set 0 if there is no icon or text. This ListItem
     * is set enabled as default.
     *
     * @param title The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item. Pass 0 for no icon.
     * @param contentDescription The a11y content description of menu item.
     * @param enabled Whether or not this menu item should be enabled.
     * @return ListItem Representing an item with text or icon.
     */
    @NonNull
    public static ListItem buildMenuListItem(
            @NonNull String title,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            @Nullable String contentDescription,
            boolean enabled) {
        PropertyModel.Builder builder =
                getListItemPropertyBuilder()
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, menuId)
                        .with(ListMenuItemProperties.START_ICON_ID, startIconId)
                        .with(ListMenuItemProperties.ENABLED, enabled);

        if (contentDescription != null) {
            builder.with(ListMenuItemProperties.CONTENT_DESCRIPTION, contentDescription);
        }
        return new MVCListAdapter.ListItem(ListMenuItemType.MENU_ITEM, builder.build());
    }

    /**
     * Helper function to build a list menu item. Pass 0 for attributes that aren't applicable to
     * the menu item (e.g. if there is no icon or text).
     *
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item. Pass 0 for no icon.
     * @param iconTintColorStateList The appearance of the icon in incognito mode.
     * @param textAppearanceStyle The appearance of the text in the menu item in incognito mode.
     * @param isIncognito Whether the current menu item will be displayed in incognito mode.
     * @param enabled Whether or not this menu item should be enabled.
     * @return ListItem Representing an item with text and maybe an icon.
     */
    @NonNull
    public static ListItem buildMenuListItemWithIncognitoBranding(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            @ColorRes int iconTintColorStateList,
            @StyleRes int textAppearanceStyle,
            boolean isIncognito,
            boolean enabled) {
        PropertyModel.Builder builder =
                getListItemPropertyBuilder()
                        .with(ListMenuItemProperties.TITLE_ID, titleId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, menuId)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.START_ICON_ID, startIconId);

        if (isIncognito) {
            builder.with(ListMenuItemProperties.TEXT_APPEARANCE_ID, textAppearanceStyle);
            builder.with(
                    ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, iconTintColorStateList);
        }
        return new MVCListAdapter.ListItem(ListMenuItemType.MENU_ITEM, builder.build());
    }

    /**
     * Helper function to build a list menu item. Pass 0 for attributes that aren't applicable to
     * the menu item (e.g. if there is no icon or text).
     *
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param endIconId The icon on the end of the menu item. Pass 0 for no icon.
     * @param enabled Whether or not this menu item should be enabled.
     * @return ListItem Representing an item with text or icon.
     */
    @NonNull
    public static ListItem buildMenuListItemWithEndIcon(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int endIconId,
            boolean enabled) {
        return new ListItem(
                ListMenuItemType.MENU_ITEM,
                getListItemPropertyBuilder()
                        .with(ListMenuItemProperties.TITLE_ID, titleId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, menuId)
                        .with(ListMenuItemProperties.END_ICON_ID, endIconId)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .build());
    }

    // Internal helper function to build a property model of list menu item.
    @NonNull
    private static PropertyModel buildPropertyModel(
            @StringRes int titleId, @IdRes int menuId, @DrawableRes int iconId, boolean enabled) {
        return getListItemPropertyBuilder()
                .with(ListMenuItemProperties.TITLE_ID, titleId)
                .with(ListMenuItemProperties.MENU_ITEM_ID, menuId)
                .with(ListMenuItemProperties.START_ICON_ID, iconId)
                .with(ListMenuItemProperties.ENABLED, enabled)
                .build();
    }

    /**
     * Return a list menu item property builder with universal properties already set e.g the text
     * appearance & icon tint state list.
     */
    @NonNull
    private static PropertyModel.Builder getListItemPropertyBuilder() {
        return new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                .with(
                        ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                        getDefaultIconTintColorStateListId())
                .with(ListMenuItemProperties.TEXT_APPEARANCE_ID, getDefaultTextAppearanceStyle());
    }
}
