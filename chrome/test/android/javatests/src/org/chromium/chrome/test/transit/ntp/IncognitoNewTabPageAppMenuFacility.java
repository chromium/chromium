// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabbed_mode.TabbedAppMenuPropertiesDelegate;
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

        boolean isSubmenusEnabled =
                TabbedAppMenuPropertiesDelegate.isSubmenusEnabled(mHostStation.getActivity());

        if (isSubmenusEnabled) {
            mAddToGroup = declarePossibleMenuItem(items, TAB_GROUPS_PARENT_ID);
        } else {
            mAddToGroup = declareMenuItem(items, ADD_TO_GROUP_ID);
        }

        mNewWindow = declarePossibleMenuItem(items, NEW_WINDOW_ID);
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            mNewIncognitoWindow = declareMenuItem(items, NEW_INCOGNITO_WINDOW_ID);
        }

        if (isSubmenusEnabled) {
            declarePossibleMenuItem(items, HISTORY_PARENT_ID);
            mBookmarksParent = declareMenuItem(items, BOOKMARKS_PARENT_ID);
            declarePossibleMenuItem(items, HELP_PARENT_ID);
            declarePossibleMenuItem(items, SAVE_AND_SHARE_PARENT_ID);
            declarePossibleMenuItem(items, DOWNLOADS_ID);
        } else {
            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                declareAbsentMenuItem(items, HISTORY_ID);
            } else {
                declareMenuItem(items, HISTORY_ID);
            }
            mBookmarks = declareMenuItem(items, BOOKMARKS_ID);
            declareMenuItem(items, DOWNLOADS_ID);
            declareMenuItem(items, HELP_AND_FEEDBACK_ID);
        }

        declareAbsentMenuItem(items, DELETE_BROWSING_DATA_ID);
        declareAbsentMenuItem(items, RECENT_TABS_ID);

        mSettings = declareMenuItem(items, SETTINGS_ID);
    }
}
