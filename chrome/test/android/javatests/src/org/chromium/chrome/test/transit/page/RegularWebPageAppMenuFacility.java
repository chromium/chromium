// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;

/** The app menu shown when pressing ("...") in a regular Tab showing a web page. */
public class RegularWebPageAppMenuFacility extends PageAppMenuFacility<WebPageStation> {
    public Item mQuickDelete;

    @Override
    protected void declareItems(ItemsBuilder items) {
        mNewTab = declareMenuItem(items, NEW_TAB_ID);
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);
        }

        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            mAddToGroup = declareMenuItem(items, ADD_TO_GROUP_ID);
        }

        mNewWindow = declarePossibleMenuItem(items, NEW_WINDOW_ID);
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            mNewIncognitoWindow = declareMenuItem(items, NEW_INCOGNITO_WINDOW_ID);
        }

        declareMenuItem(items, HISTORY_ID);
        mQuickDelete = declareMenuItem(items, DELETE_BROWSING_DATA_ID);
        declareMenuItem(items, DOWNLOADS_ID);
        mBookmarks = declareMenuItem(items, BOOKMARKS_ID);
        declareMenuItem(items, RECENT_TABS_ID);

        declareMenuItem(items, SHARE_ID);
        declareMenuItem(items, FIND_IN_PAGE_ID);
        declarePossibleStubMenuItem(items, TRANSLATE_ID);
        mReaderMode = declarePossibleMenuItem(items, READER_MODE_ID);

        // At most one of these exist.
        declarePossibleStubMenuItem(items, ADD_TO_HOME_SCREEN_UNIVERSAL_INSTALL_ID);
        declarePossibleStubMenuItem(items, OPEN_WEBAPK_ID);

        declarePossibleStubMenuItem(items, DESKTOP_SITE_ID);

        mSettings = declareMenuItem(items, SETTINGS_ID);
        declareMenuItem(items, HELP_AND_FEEDBACK_ID);
    }

    /** Select "Clear browsing data" from the app menu. */
    public QuickDeleteDialogFacility clearBrowsingData() {
        return mQuickDelete.scrollToAndSelectTo().enterFacility(createQuickDeleteDialogFacility());
    }
}
