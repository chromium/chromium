// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

/** The app menu shown when pressing ("...") in a regular NTP. */
public class NewTabPageRegularAppMenuFacility extends PageAppMenuFacility<NewTabPageStation> {
    public NewTabPageRegularAppMenuFacility(NewTabPageStation station) {
        super(station);
    }

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

        mSettings = declareMenuItemToStation(items, SETTINGS_ID, this::createSettingsStation);
        declareStubMenuItem(items, HELP_AND_FEEDBACK_ID);
    }
}
