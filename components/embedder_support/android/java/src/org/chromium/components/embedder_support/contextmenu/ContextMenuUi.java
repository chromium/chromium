// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/**
 * A representation of the ContextMenu UI. Given a list of items it should populate and display a
 * context menu.
 */
public interface ContextMenuUi {
    /**
     * Shows the Context Menu.
     * @param window Used to inflate the context menu.
     * @param webContents The WebContents that this context menu belongs to.
     * @param params The current parameters for the the context menu.
     * @param items The list of items that need to be displayed in the context menu items. This is
     *              taken from the return value of {@link ContextMenuPopulator#buildContextMenu(
     *              ContextMenu, Context, ContextMenuParams)}.
     * @param onItemClicked When the user has pressed an item the menuId associated with the item
     *                      is sent back through {@link Callback#onResult(Object)}. The ids that
     *                      could be called are in ids.xml.
     * @param onMenuShown After the menu is displayed this method should be called to present a
     *                    full menu.
     * @param onMenuClosed When the menu is closed, this method is called to do any possible final
     *                     clean up.
     */
    void displayMenu(
            WindowAndroid window,
            WebContents webContents,
            ContextMenuParams params,
            List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked,
            Runnable onMenuShown,
            Runnable onMenuClosed);

    /** Dismiss the context menu. */
    void dismiss();
}
