// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu.Delegate;
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
     * @param delegate The {@link Delegate} used to handle menu clicks. If not provided, the item's
     *     CLICK_LISTENER or listMenu's onMenuItemSelected method will be used.
     */
    public static BasicListMenu getBasicListMenu(
            Context context, MVCListAdapter.ModelList data, @Nullable Delegate delegate) {
        return getBasicListMenu(
                context,
                data,
                delegate,
                /* backgroundTintColorRes= */ 0,
                /* bottomHairlineColor= */ SemanticColorUtils.getDividerLineBgColor(context));
    }

    /**
     * Convenience method for constructing a {@link BasicListMenu} with the preferred content view.
     *
     * @param context The Android context.
     * @param data The data to display in the list.
     * @param delegate The {@link Delegate} used to handle menu clicks. If not provided, the item's
     *     CLICK_LISTENER or listMenu's onMenuItemSelected method will be used.
     * @param backgroundTintColorRes tint for the menu background.
     * @param bottomHairlineColor Color for the bottom hairline of the unscrollable header.
     */
    public static BasicListMenu getBasicListMenu(
            Context context,
            MVCListAdapter.ModelList data,
            @Nullable Delegate delegate,
            @ColorRes int backgroundTintColorRes,
            @Nullable @ColorInt Integer bottomHairlineColor) {
        return new BasicListMenu(
                context,
                data,
                delegate,
                R.drawable.default_popup_menu_bg,
                backgroundTintColorRes,
                bottomHairlineColor);
    }
}
