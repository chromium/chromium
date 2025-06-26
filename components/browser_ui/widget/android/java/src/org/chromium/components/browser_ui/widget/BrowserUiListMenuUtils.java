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
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

/** Collection of utility methods related to the browser UI list menus. */
@NullMarked
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
    public static BasicListMenu getBasicListMenu(
            Context context, MVCListAdapter.ModelList data, ListMenu.Delegate delegate) {
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
    public static BasicListMenu getBasicListMenu(
            Context context,
            MVCListAdapter.ModelList data,
            ListMenu.Delegate delegate,
            @ColorRes int backgroundTintColor) {
        View contentView = LayoutInflater.from(context).inflate(R.layout.app_menu_layout, null);
        ListView listView = contentView.findViewById(R.id.app_menu_list);
        return new BasicListMenu(
                context, data, contentView, listView, delegate, backgroundTintColor);
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItem(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            boolean enabled) {
        return new ListItemBuilder()
                .setTitleRes(titleId)
                .setMenuId(menuId)
                .setStartIconId(startIconId)
                .setEnabled(enabled)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItem(
            @StringRes int titleId, @IdRes int menuId, @DrawableRes int startIconId) {
        return new ListItemBuilder()
                .setTitleRes(titleId)
                .setMenuId(menuId)
                .setStartIconId(startIconId)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItem(
            String title, @IdRes int menuId, @DrawableRes int startIconId, boolean enabled) {
        return new ListItemBuilder()
                .setTitle(title)
                .setMenuId(menuId)
                .setStartIconId(startIconId)
                .setEnabled(enabled)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItem(
            String title,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            @Nullable String contentDescription,
            boolean enabled) {
        ListItemBuilder builder =
                new ListItemBuilder()
                        .setTitle(title)
                        .setMenuId(menuId)
                        .setStartIconId(startIconId)
                        .setEnabled(enabled);
        if (contentDescription != null) {
            builder.setContentDescription(contentDescription);
        }
        return builder.build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItemWithEllipsizedAtEnd(
            String title,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            boolean enabled,
            boolean isTextEllipsizedAtEnd) {
        return new ListItemBuilder()
                .setTitle(title)
                .setMenuId(menuId)
                .setStartIconId(startIconId)
                .setEnabled(enabled)
                .setIsTextEllipsizedAtEnd(isTextEllipsizedAtEnd)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItemWithIncognitoBranding(
            String title, @IdRes int menuId, boolean isIncognito, boolean enabled) {
        return new ListItemBuilder()
                .setTitle(title)
                .setMenuId(menuId)
                .setIsIncognito(isIncognito)
                .setEnabled(enabled)
                .setTextAppearanceStyle(R.style.TextAppearance_TextLarge_Primary_Baseline_Light)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItemWithIncognitoBranding(
            @StringRes int titleId, @IdRes int menuId, boolean isIncognito, boolean enabled) {
        return new ListItemBuilder()
                .setTitleRes(titleId)
                .setMenuId(menuId)
                .setTextAppearanceStyle(R.style.TextAppearance_TextLarge_Primary_Baseline_Light)
                .setIsIncognito(isIncognito)
                .setEnabled(enabled)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    public static ListItem buildMenuListItemWithIncognitoBranding(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            boolean isIncognito) {
        return new ListItemBuilder()
                .setTitleRes(titleId)
                .setMenuId(menuId)
                .setStartIconId(startIconId)
                .setIconTintColorStateList(R.color.default_icon_color_light_tint_list)
                .setTextAppearanceStyle(R.style.TextAppearance_TextLarge_Primary_Baseline_Light)
                .setIsIncognito(isIncognito)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItemWithIncognitoBranding(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            @ColorRes int iconTintColorStateList,
            @StyleRes int textAppearanceStyle,
            boolean isIncognito,
            boolean enabled) {
        return new ListItemBuilder()
                .setTitleRes(titleId)
                .setMenuId(menuId)
                .setStartIconId(startIconId)
                .setIconTintColorStateList(iconTintColorStateList)
                .setTextAppearanceStyle(textAppearanceStyle)
                .setIsIncognito(isIncognito)
                .setEnabled(enabled)
                .build();
    }

    /**
     * @deprecated Use {@link ListItemBuilder} instead.
     */
    @Deprecated
    public static ListItem buildMenuListItemWithEndIcon(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int endIconId,
            boolean enabled) {
        return new ListItemBuilder()
                .setTitleRes(titleId)
                .setMenuId(menuId)
                .setEndIconId(endIconId)
                .setEnabled(enabled)
                .build();
    }
}
