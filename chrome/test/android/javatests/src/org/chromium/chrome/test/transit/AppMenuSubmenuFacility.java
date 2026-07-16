// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import android.widget.ListView;

import androidx.annotation.IdRes;

import org.chromium.base.test.transit.RootSpec;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;

/** Facility representing an opened App Menu Submenu / Flyout popup. */
public class AppMenuSubmenuFacility<HostStationT extends Station<?>>
        extends ScrollableFacility<HostStationT> {

    protected Item mBookmarkThisPage;
    protected Item mBookmarks;

    public AppMenuSubmenuFacility() {
        declareContainerView(
                ListView.class,
                allOf(withId(R.id.app_menu_list), isDisplayed()),
                ViewElement.newOptions().rootSpec(RootSpec.focusedRoot()).build());
    }

    /** Create a new app menu item inside submenu. */
    protected Item declareMenuItem(ItemsBuilder items, @IdRes int id) {
        return items.declareItem(withId(id), AppMenuFacility.withMenuItemId(id));
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        mBookmarkThisPage = declareMenuItem(items, AppMenuFacility.BOOKMARK_THIS_PAGE_ID);
        mBookmarks = declareMenuItem(items, AppMenuFacility.BOOKMARKS_ID);
    }

    public Item getBookmarksItem() {
        return mBookmarks;
    }
}
