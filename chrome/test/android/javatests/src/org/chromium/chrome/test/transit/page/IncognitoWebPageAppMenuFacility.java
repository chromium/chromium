// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;

/** The app menu shown when pressing ("...") in an Incognito Tab showing a web page. */
public class IncognitoWebPageAppMenuFacility extends PageAppMenuFacility<WebPageStation> {
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

        declareMenuItem(items, SHARE_ID);
        declareMenuItem(items, FIND_IN_PAGE_ID);
        declarePossibleStubMenuItem(items, TRANSLATE_ID);
        mReaderMode = declarePossibleMenuItem(items, READER_MODE_ID);

        // None of these should exist.
        declareAbsentMenuItem(items, ADD_TO_HOME_SCREEN_UNIVERSAL_INSTALL_ID);
        declareAbsentMenuItem(items, OPEN_WEBAPK_ID);

        declarePossibleStubMenuItem(items, DESKTOP_SITE_ID);

        mSettings = declareMenuItem(items, SETTINGS_ID);
        declareMenuItem(items, HELP_AND_FEEDBACK_ID);
    }
}
