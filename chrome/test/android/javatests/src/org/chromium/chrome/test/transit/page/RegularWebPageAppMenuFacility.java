// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

/** The app menu shown when pressing ("...") in a regular Tab showing a web page. */
public class RegularWebPageAppMenuFacility extends PageAppMenuFacility<WebPageStation> {
    @Override
    protected void declareItems(ItemsBuilder items) {
        mNewTab = declareMenuItemToStation(items, NEW_TAB_ID, this::createNewTabPageStation);
        mNewIncognitoTab =
                declareMenuItemToStation(
                        items, NEW_INCOGNITO_TAB_ID, this::createIncognitoNewTabPageStation);

        declareStubMenuItem(items, HISTORY_ID);
        declareStubMenuItem(items, DELETE_BROWSING_DATA_ID);
        declareStubMenuItem(items, DOWNLOADS_ID);
        declareStubMenuItem(items, BOOKMARKS_ID);
        declareStubMenuItem(items, RECENT_TABS_ID);

        declareStubMenuItem(items, SHARE_ID);
        declareStubMenuItem(items, FIND_IN_PAGE_ID);
        declarePossibleStubMenuItem(items, TRANSLATE_ID);

        // At most one of these exist.
        declarePossibleStubMenuItem(items, ADD_TO_HOME_SCREEN__UNIVERSAL_INSTALL__ID);
        declarePossibleStubMenuItem(items, OPEN_WEBAPK_ID);

        declarePossibleStubMenuItem(items, DESKTOP_SITE_ID);

        mSettings = declareMenuItemToStation(items, SETTINGS_ID, this::createSettingsStation);
        declareStubMenuItem(items, HELP_AND_FEEDBACK_ID);
    }
}
