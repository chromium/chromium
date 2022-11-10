// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class used by {@link MessageWrapper} to manage the message secondary menu and menu items.
 */
public class MessageSecondaryMenuItems {
    @VisibleForTesting
    ModelList mMenuItems = new ModelList();

    /**
     * Creates and returns a {@link ListMenu} populated by |mMenuItems|.
     * @param context The current context.
     * @param delegate The {@link ListMenu.Delegate} that handles a menu item click.
     * @return A {@link ListMenu} populated by |mMenuItems|.
     */
    ListMenu createListMenu(Context context, ListMenu.Delegate delegate) {
        return new BasicListMenu(context, mMenuItems, delegate);
    }

    /**
     * Add an item to the list menu.
     * @param itemId The list menu item ID.
     * @param resourceId The start icon resource ID of the list menu item.
     * @param itemText The title of the list menu item.
     */
    PropertyModel addMenuItem(int itemId, int resourceId, String itemText) {
        PropertyModel item = new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                     .with(ListMenuItemProperties.MENU_ITEM_ID, itemId)
                                     .with(ListMenuItemProperties.START_ICON_ID, resourceId)
                                     .with(ListMenuItemProperties.TITLE, itemText)
                                     .with(ListMenuItemProperties.ENABLED, true)
                                     .build();
        mMenuItems.add(new ListItem(ListMenuItemType.MENU_ITEM, item));
        return item;
    }

    /**
     * Add an item to the list menu.
     * @param itemId The list menu item ID.
     * @param resourceId The start icon resource ID of the list menu item.
     * @param itemText The title of the list menu item.
     * @param itemDescription The a11y content description of menu item.
     * Set empty string/NULL to unset content description.
     */
    PropertyModel addMenuItem(int itemId, int resourceId, String itemText, String itemDescription) {
        PropertyModel item =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, itemId)
                        .with(ListMenuItemProperties.START_ICON_ID, resourceId)
                        .with(ListMenuItemProperties.TITLE, itemText)
                        .with(ListMenuItemProperties.CONTENT_DESCRIPTION, itemDescription)
                        .with(ListMenuItemProperties.ENABLED, true)
                        .build();
        mMenuItems.add(new ListItem(ListMenuItemType.MENU_ITEM, item));
        return item;
    }

    /**
     * Remove all items from the list menu.
     */
    void clearMenuItems() {
        mMenuItems.clear();
    }

    /**
     * Add a divider to the list menu.
     */
    void addMenuDivider() {
        mMenuItems.add(new ListItem(ListMenuItemType.DIVIDER, new PropertyModel()));
    }
}
