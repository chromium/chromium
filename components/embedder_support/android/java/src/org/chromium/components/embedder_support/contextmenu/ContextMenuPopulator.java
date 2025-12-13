// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/**
 * A delegate responsible for populating context menus and processing results from
 * ContextMenuHelper.
 */
@NullMarked
public interface ContextMenuPopulator {
    /**
     * Should be used to populate {@code menu} with the correct context menu items.
     *
     * @return A list of groups. If the link contains multiple items (like an image link), the list
     *     will have both an image group and a link group. The groups will be separated from each
     *     other by horizontal dividers.
     */
    List<ModelList> buildContextMenu();

    /**
     * Called when a context menu item has been selected.
     *
     * @param itemId The id of the selected menu item.
     * @return Whether or not the selection was handled.
     */
    boolean onItemSelected(int itemId);

    /** Called when the context menu is closed. */
    void onMenuClosed();

    /** Determines whether the the containing browser is switched to incognito mode. */
    boolean isIncognito();

    /**
     * @return The title of current web page.
     */
    String getPageTitle();

    /**
     * @return A chip delegate responsible for populating chip data and action handling.
     */
    @Nullable ChipDelegate getChipDelegate();

    /**
     * @return Whether the populator has custom items to show in the context menu. Defaults to false
     *     and can be overridden by implementations.
     */
    default boolean hasCustomItems() {
        return false;
    }
}
