// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.test.transit.page.PageAppMenuFacility;

/** The app menu shown when pressing ("...") in a Incognito NTP. */
public class IncognitoNewTabPageAppMenuFacility
        extends PageAppMenuFacility<IncognitoNewTabPageStation> {
    @Override
    protected void declareItems(ItemsBuilder items) {
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            mNewTab = declareMenuItem(items, NEW_TAB_ID);
        }
        mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);

        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            mAddToGroup = declareMenuItem(items, ADD_TO_GROUP_ID);
        }

        mNewWindow = declarePossibleMenuItem(items, NEW_WINDOW_ID);
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            mNewIncognitoWindow = declareMenuItem(items, NEW_INCOGNITO_WINDOW_ID);
        }

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            declareAbsentMenuItem(items, HISTORY_ID);
        } else {
            declareMenuItem(items, HISTORY_ID);
        }
        declareAbsentMenuItem(items, DELETE_BROWSING_DATA_ID);

        declareMenuItem(items, DOWNLOADS_ID);
        mBookmarks = declareMenuItem(items, BOOKMARKS_ID);
        declareAbsentMenuItem(items, RECENT_TABS_ID);

        mSettings = declareMenuItem(items, SETTINGS_ID);
        declareMenuItem(items, HELP_AND_FEEDBACK_ID);
    }
}
