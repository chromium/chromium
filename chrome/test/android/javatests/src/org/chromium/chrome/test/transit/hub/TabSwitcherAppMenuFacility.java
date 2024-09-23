// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.AppMenuFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;

import java.util.Collections;

/** The app menu shown when pressing ("...") in the Hub on a tab switcher pane. */
public class TabSwitcherAppMenuFacility extends AppMenuFacility<TabSwitcherStation> {
    public static final int CLOSE_ALL_TABS_ID = R.id.close_all_tabs_menu_id;
    public static final int CLOSE_INCOGNITO_TABS_ID = R.id.close_all_incognito_tabs_menu_id;
    public static final int SELECT_TABS_ID = R.id.menu_select_tabs;

    private final boolean mIsIncognito;
    private Item<RegularNewTabPageStation> mNewTab;
    private Item<IncognitoNewTabPageStation> mNewIncognitoTab;
    private Item<Void> mCloseAllTabs;
    private Item<Void> mCloseIncognitoTabs;
    private Item<TabSwitcherListEditorFacility> mSelectTabs;
    private Item<Void> mClearBrowsingData;
    private Item<SettingsStation> mSettings;

    public TabSwitcherAppMenuFacility(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        boolean isTablet = mHostStation.getActivity().isTablet();

        mNewTab = declareMenuItemToStation(items, NEW_TAB_ID, this::createNewTabPageStation);
        mNewIncognitoTab =
                declareMenuItemToStation(
                        items, NEW_INCOGNITO_TAB_ID, this::createIncognitoNewTabPageStation);
        if (!mIsIncognito) {
            // Regular Hub Tab Switcher
            int tabCount =
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    mHostStation
                                            .getActivity()
                                            .getTabModelSelector()
                                            .getModel(/* incognito= */ false)
                                            .getCount());
            if (tabCount > 0) {
                mCloseAllTabs = declareStubMenuItem(items, CLOSE_ALL_TABS_ID);
                mSelectTabs =
                        declareMenuItemToFacility(
                                items, SELECT_TABS_ID, this::createListEditorFacility);
            } else {
                // Empty state. In tablets the following items are not displayed, while in phones
                // they are disabled.
                if (isTablet) {
                    mCloseAllTabs = declareAbsentMenuItem(items, CLOSE_ALL_TABS_ID);
                    declareAbsentMenuItem(items, SELECT_TABS_ID);
                } else {
                    mCloseAllTabs = declareDisabledMenuItem(items, CLOSE_ALL_TABS_ID);
                    declareDisabledMenuItem(items, SELECT_TABS_ID);
                }
            }
            mClearBrowsingData = declareStubMenuItem(items, DELETE_BROWSING_DATA_ID);
        } else {
            // Incognito Hub Tab Switcher

            // If there are no incognito tabs, the incognito tab switcher pane disappears so
            // "Close Incognito Tabs" and "Select tabs" are always present and
            // enabled.
            mCloseIncognitoTabs = declareStubMenuItem(items, CLOSE_INCOGNITO_TABS_ID);
            mSelectTabs =
                    declareMenuItemToFacility(
                            items, SELECT_TABS_ID, this::createListEditorFacility);
        }
        mSettings = declareMenuItemToStation(items, SETTINGS_ID, this::createSettingsStation);
    }

    /** Select "New tab" from the app menu. */
    public RegularNewTabPageStation openNewTab() {
        return mNewTab.scrollToAndSelect();
    }

    /** Select "New Incognito tab" from the app menu. */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        return mNewIncognitoTab.scrollToAndSelect();
    }

    /** Select "Settings" from the app menu. */
    public SettingsStation openSettings() {
        return mSettings.scrollToAndSelect();
    }

    /** Select "Select tabs" from the app menu. */
    public TabSwitcherListEditorFacility clickSelectTabs() {
        return mSelectTabs.scrollToAndSelect();
    }

    private TabSwitcherListEditorFacility createListEditorFacility() {
        return new TabSwitcherListEditorFacility(Collections.EMPTY_LIST);
    }
}
