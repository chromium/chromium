// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.modelutil.MVCListAdapter;

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
}
