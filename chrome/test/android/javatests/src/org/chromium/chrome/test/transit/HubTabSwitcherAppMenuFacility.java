// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.chrome.R;

import java.util.List;

/** The app menu shown when pressing ("...") in the Hub on a tab switcher pane. */
public class HubTabSwitcherAppMenuFacility extends AppMenuFacility<HubTabSwitcherBaseStation> {
    public static final int CLOSE_ALL_TABS_ID = R.id.close_all_tabs_menu_id;
    public static final int CLOSE_INCOGNITO_TABS_ID = R.id.close_all_incognito_tabs_menu_id;
    public static final int SELECT_TABS_ID = R.id.menu_select_tabs;
    public static final int CLEAR_BROWSING_DATA_ID = R.id.quick_delete_menu_id;

    private final boolean mIsIncognito;
    private Item<NewTabPageStation> mNewTab;
    private Item<IncognitoNewTabPageStation> mNewIncognitoTab;
    private Item<Void> mCloseAllTabs;
    private Item<Void> mCloseIncognitoTabs;
    private Item<HubTabSwitcherListEditorFacility> mSelectTabs;
    private Item<Void> mClearBrowsingData;
    private Item<SettingsStation> mSettings;

    public HubTabSwitcherAppMenuFacility(HubTabSwitcherBaseStation station, boolean isIncognito) {
        super(station, station.mChromeTabbedActivityTestRule);
        mIsIncognito = isIncognito;
    }

    @Override
    protected void declareItems(List<Item<?>> items) {
        boolean isTablet = mChromeTabbedActivityTestRule.getActivity().isTablet();

        mNewTab = newMenuItemToStation(NEW_TAB_ID, this::createNewTabPageStation);
        mNewIncognitoTab =
                newMenuItemToStation(NEW_INCOGNITO_TAB_ID, this::createIncognitoNewTabPageStation);
        mSettings = newMenuItemToStation(SETTINGS_ID, this::createSettingsStation);
        if (!mIsIncognito) {
            // Regular Hub Tab Switcher

            Item<?> selectTabs;
            if (mChromeTabbedActivityTestRule.tabsCount(/* regular= */ false) > 0) {
                mCloseAllTabs = newStubMenuItem(CLOSE_ALL_TABS_ID);
                mSelectTabs = newMenuItemToFacility(SELECT_TABS_ID, this::createListEditorFacility);
                selectTabs = mSelectTabs;
            } else {
                // Empty state. In tablets the following items are not displayed, while in phones
                // they are disabled.
                if (isTablet) {
                    mCloseAllTabs = newAbsentMenuItem(CLOSE_ALL_TABS_ID);
                    selectTabs = newAbsentMenuItem(SELECT_TABS_ID);
                } else {
                    mCloseAllTabs = newDisabledMenuItem(CLOSE_ALL_TABS_ID);
                    selectTabs = newDisabledMenuItem(SELECT_TABS_ID);
                }
            }
            mClearBrowsingData = newStubMenuItem(CLEAR_BROWSING_DATA_ID);

            items.add(mNewTab);
            items.add(mNewIncognitoTab);
            items.add(mCloseAllTabs);
            items.add(selectTabs);
            items.add(mClearBrowsingData);
            items.add(mSettings);
        } else {
            // Incognito Hub Tab Switcher

            // If there are no incognito tabs, the incognito tab switcher pane disappears so
            // "Close Incognito Tabs" and "Select tabs" are always present and
            // enabled.
            mCloseIncognitoTabs = newStubMenuItem(CLOSE_INCOGNITO_TABS_ID);
            mSelectTabs = newMenuItemToFacility(SELECT_TABS_ID, this::createListEditorFacility);

            items.add(mNewTab);
            items.add(mNewIncognitoTab);
            items.add(mCloseIncognitoTabs);
            items.add(mSelectTabs);
            items.add(mSettings);
        }
    }

    /** Select "New tab" from the app menu. */
    public NewTabPageStation openNewTab() {
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
    public HubTabSwitcherListEditorFacility clickSelectTabs() {
        return mSelectTabs.scrollToAndSelect();
    }

    private HubTabSwitcherListEditorFacility createListEditorFacility() {
        return new HubTabSwitcherListEditorFacility(mHostStation, mChromeTabbedActivityTestRule);
    }
}
