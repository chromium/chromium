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
    public Item mQuickDelete;

    @Override
    protected void declareItems(ItemsBuilder items) {
        mNewTab = declareMenuItem(items, NEW_TAB_ID);
        mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);
        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            mAddToGroup = declareMenuItem(items, ADD_TO_GROUP_ID);
        }
        if (ChromeFeatureList.sAndroidPinnedTabs.isEnabled()) {
            // At most one of these exist.
            mPinTab = declarePossibleMenuItem(items, PIN_TAB);
            mUnpinTab = declarePossibleMenuItem(items, UNPIN_TAB);
        }
        mNewWindow = declarePossibleMenuItem(items, NEW_WINDOW_ID);

        declareMenuItem(items, HISTORY_ID);
        mQuickDelete = declareMenuItem(items, DELETE_BROWSING_DATA_ID);

        declareMenuItem(items, DOWNLOADS_ID);
        declareMenuItem(items, BOOKMARKS_ID);
        declareMenuItem(items, RECENT_TABS_ID);

        mSettings = declareMenuItem(items, SETTINGS_ID);
        declareMenuItem(items, HELP_AND_FEEDBACK_ID);
    }

    /** Select "Clear browsing data" from the app menu. */
    public QuickDeleteDialogFacility clearBrowsingData() {
        return mQuickDelete.scrollToAndSelectTo().enterFacility(createQuickDeleteDialogFacility());
    }
}
