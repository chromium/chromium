// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/**
 * A delegate responsible for populating context menus and processing results from
 * ContextMenuHelper.
 */
public interface ContextMenuPopulator {
    /**
     * Should be used to populate {@code menu} with the correct context menu items.
     *
     * @return A list separate by groups. Each "group" will contain items related to said group as
     *     well as an integer that is a string resource for the group. Image items will have items
     *     that belong to that are related to that group and the string resource for the group will
     *     likely say "IMAGE". If the link pressed is contains multiple items (like an image link)
     *     the list will have both an image list and a link list.
     */
    List<Pair<Integer, ModelList>> buildContextMenu();

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
    @Nullable
    ChipDelegate getChipDelegate();
}
