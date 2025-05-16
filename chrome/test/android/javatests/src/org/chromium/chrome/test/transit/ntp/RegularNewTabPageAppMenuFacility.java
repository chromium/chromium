// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.transit.page.PageAppMenuFacility;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;

/** The app menu shown when pressing ("...") in a regular NTP. */
public class RegularNewTabPageAppMenuFacility
        extends PageAppMenuFacility<RegularNewTabPageStation> {
    public Item<QuickDeleteDialogFacility> mQuickDelete;

    @Override
    protected void declareItems(ItemsBuilder items) {
        mNewTab = declareMenuItemToStation(items, NEW_TAB_ID, this::createNewTabPageStation);
        mNewIncognitoTab =
                declareMenuItemToStation(
                        items, NEW_INCOGNITO_TAB_ID, this::createIncognitoNewTabPageStation);
        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            mAddToGroup =
                    declareMenuItemToFacility(
                            items, ADD_TO_GROUP_ID, this::createTabGroupListBottomSheetFacility);
        }
        mNewWindow = declarePossibleMenuItem(items, NEW_WINDOW_ID, this::handleOpenNewWindow);

        declareStubMenuItem(items, HISTORY_ID);
        mQuickDelete =
                declareMenuItemToFacility(
                        items, DELETE_BROWSING_DATA_ID, this::createQuickDeleteDialogFacility);

        declareStubMenuItem(items, DOWNLOADS_ID);
        declareStubMenuItem(items, BOOKMARKS_ID);
        declareStubMenuItem(items, RECENT_TABS_ID);

        mSettings = declareMenuItemToStation(items, SETTINGS_ID, this::createSettingsStation);
        declareStubMenuItem(items, HELP_AND_FEEDBACK_ID);
    }

    /** Select "Clear browsing data" from the app menu. */
    public QuickDeleteDialogFacility clearBrowsingData() {
        return mQuickDelete.scrollToAndSelect();
    }
}
