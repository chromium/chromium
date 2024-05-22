// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View.OnLongClickListener;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * The coordinator responsible for showing the Tab Switcher Action Menu on the Start Surface.
 *
 * <p>Since the Start Surface is not a tab, this removes the close tab option from the parent class
 * {@link TabSwitcherActionMenuCoordinator}.
 */
public class StartSurfaceTabSwitcherActionMenuCoordinator extends TabSwitcherActionMenuCoordinator {
    /**
     * @param onItemClicked The clicked listener handling clicks on TabSwitcherActionMenu.
     * @param profile The {@link Profile} associated with the tabs.
     * @return a long click listener of the long press action of tab switcher button.
     */
    public static OnLongClickListener createOnLongClickListener(
            Callback<Integer> onItemClicked, Profile profile) {
        return createOnLongClickListener(
                new StartSurfaceTabSwitcherActionMenuCoordinator(profile), onItemClicked);
    }

    /** Construct a coordinator for the given {@link Profile}. */
    public StartSurfaceTabSwitcherActionMenuCoordinator(Profile profile) {
        super(profile);
    }

    @Override
    ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_TAB));
        itemList.add(buildListItemByMenuItemType(MenuItemType.NEW_INCOGNITO_TAB));
        return itemList;
    }
}
