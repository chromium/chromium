// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;

/** The app menu shown when pressing ("...") in the Hub on a tab group card. */
public class TabSwitcherGroupCardAppMenuFacility extends ScrollableFacility<TabSwitcherStation> {
    public static final Matcher<View> CLOSE_TAB_GROUP_MENU_ITEM_MATCHER =
            allOf(
                    withText(R.string.close_tab_group_menu_item),
                    isDescendantOfA(withId(R.id.tab_group_action_menu_list)));

    private final boolean mIsIncognito;
    private final String mTitle;
    private Item<UndoSnackbarFacility> mCloseRegularTabGroup;

    public TabSwitcherGroupCardAppMenuFacility(boolean isIncognito, String title) {
        mIsIncognito = isIncognito;
        mTitle = title;
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        if (!mIsIncognito) {
            mCloseRegularTabGroup =
                    items.declareItem(
                            CLOSE_TAB_GROUP_MENU_ITEM_MATCHER, null, this::doCloseRegularTabGroup);
        }
    }

    @Override
    public int getMinimumOnScreenItemCount() {
        // Expect at least the first two menu items, it's enough to establish the transition is
        // done.
        return 2;
    }

    /** Select "Close" from the tab group overflow menu to close (hide) the tab group. */
    public UndoSnackbarFacility closeRegularTabGroup() {
        return mCloseRegularTabGroup.scrollToAndSelect();
    }

    private UndoSnackbarFacility doCloseRegularTabGroup(
            ItemOnScreenFacility<UndoSnackbarFacility> itemOnScreen) {
        int tabCount = mHostStation.getActivity().getTabModelSelector().getTotalTabCount();
        String snackbarMessage = TabGroupUtil.getUndoCloseGroupSnackbarMessageString(mTitle);
        UndoSnackbarFacility undoSnackbar = new UndoSnackbarFacility(snackbarMessage);
        mHostStation.swapFacilitySync(this, undoSnackbar, itemOnScreen.clickTrigger());
        return undoSnackbar;
    }
}
