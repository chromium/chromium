// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;

/** The app menu shown when pressing ("...") in the Hub on a tab group card. */
public class TabSwitcherGroupCardAppMenuFacility extends ScrollableFacility<TabSwitcherStation> {
    private final boolean mIsIncognito;
    private final String mTitle;
    public final ViewElement<View> menuListElement;
    private Item<UndoSnackbarFacility> mCloseRegularTabGroup;

    public TabSwitcherGroupCardAppMenuFacility(boolean isIncognito, String title) {
        mIsIncognito = isIncognito;
        mTitle = title;
        menuListElement = declareView(withId(R.id.tab_group_action_menu_list));
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        if (!mIsIncognito) {
            mCloseRegularTabGroup =
                    items.declareItem(
                            menuListElement.descendant(
                                    withText(R.string.close_tab_group_menu_item)),
                            /* offScreenDataMatcher= */ null,
                            this::doCloseRegularTabGroup);
        }
    }

    /** Select "Close" from the tab group overflow menu to close (hide) the tab group. */
    public UndoSnackbarFacility closeRegularTabGroup() {
        return mCloseRegularTabGroup.scrollToAndSelect();
    }

    private UndoSnackbarFacility doCloseRegularTabGroup(
            ItemOnScreenFacility<UndoSnackbarFacility> itemOnScreen) {
        String snackbarMessage = TabGroupUtil.getUndoCloseGroupSnackbarMessageString(mTitle);
        UndoSnackbarFacility undoSnackbar = new UndoSnackbarFacility(snackbarMessage);
        mHostStation.swapFacilitySync(
                this, undoSnackbar, itemOnScreen.viewElement.getClickTrigger());
        return undoSnackbar;
    }
}
