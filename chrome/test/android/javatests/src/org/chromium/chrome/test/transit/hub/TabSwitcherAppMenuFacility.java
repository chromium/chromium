// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Station;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.test.transit.CtaAppMenuFacility;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.chrome.test.transit.settings.SettingsStation;

import java.util.Collections;

/**
 * The app menu shown when pressing ("...") in the Hub on a tab switcher pane.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public class TabSwitcherAppMenuFacility<HostStationT extends TabSwitcherStation>
        extends CtaAppMenuFacility<HostStationT> {
    public static final int CLOSE_ALL_TABS_ID = R.id.close_all_tabs_menu_id;
    public static final int CLOSE_INCOGNITO_TABS_ID = R.id.close_all_incognito_tabs_menu_id;
    public static final int SELECT_TABS_ID = R.id.menu_select_tabs;

    private final boolean mIsIncognito;
    private @Nullable Item mNewTab;
    private @Nullable Item mNewIncognitoTab;
    private @Nullable Item mNewWindow;
    private @Nullable Item mNewIncognitoWindow;
    private @Nullable Item mNewTabGroup;
    private Item mCloseAllTabs;
    private Item mCloseIncognitoTabs;
    private Item mSelectTabs;
    private Item mQuickDelete;
    private Item mSettings;

    public TabSwitcherAppMenuFacility(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        boolean isTablet = mHostStation.getActivity().isTablet();

        if (!mIsIncognito) {
            // Regular Hub Tab Switcher
            mNewTab = declareMenuItem(items, NEW_TAB_ID);
            if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);
            }

            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                mNewWindow = declareMenuItem(items, NEW_WINDOW_ID);
                mNewIncognitoWindow = declareMenuItem(items, NEW_INCOGNITO_WINDOW_ID);
            }

            if (ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled()) {
                mNewTabGroup = declareMenuItem(items, NEW_TAB_GROUP_ID);
            }

            int tabCount =
                    ThreadUtils.runOnUiThreadBlocking(() -> mHostStation.getTabModel().getCount());
            if (tabCount > 0) {
                mCloseAllTabs = declareMenuItem(items, CLOSE_ALL_TABS_ID);
                mSelectTabs = declareMenuItem(items, SELECT_TABS_ID);
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
            mQuickDelete = declareMenuItem(items, DELETE_BROWSING_DATA_ID);
        } else {
            // Incognito Hub Tab Switcher
            if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                mNewTab = declareMenuItem(items, NEW_TAB_ID);
            }
            mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);

            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                mNewWindow = declareMenuItem(items, NEW_WINDOW_ID);
                mNewIncognitoWindow = declareMenuItem(items, NEW_INCOGNITO_WINDOW_ID);
            }

            if (ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled()) {
                mNewTabGroup = declareMenuItem(items, NEW_TAB_GROUP_ID);
            }

            // If there are no incognito tabs, the incognito tab switcher pane disappears so
            // "Close Incognito Tabs" and "Select tabs" are always present and
            // enabled.
            mCloseIncognitoTabs = declareMenuItem(items, CLOSE_INCOGNITO_TABS_ID);
            mSelectTabs = declareMenuItem(items, SELECT_TABS_ID);
        }
        mSettings = declareMenuItem(items, SETTINGS_ID);
    }

    /** Select "New tab" from the app menu. */
    public RegularNewTabPageStation openNewTab() {
        return mNewTab.scrollToAndSelectTo().arriveAt(createNewTabPageStation());
    }

    /** Select "New Incognito tab" or "New Incognito Window" from the app menu. */
    public IncognitoNewTabPageStation openNewIncognitoTabOrWindow() {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            return mNewIncognitoWindow
                    .scrollToAndSelectTo()
                    .inNewTask()
                    .arriveAt(createNewIncognitoWindowStation());
        } else {
            return mNewIncognitoTab
                    .scrollToAndSelectTo()
                    .arriveAt(createNewIncognitoTabPageStation());
        }
    }

    /** Select "New tab group" from the app menu. */
    public NewTabGroupDialogFacility<HostStationT> openNewTabGroup() {
        assertTrue(ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled());
        assertNotNull(mNewTabGroup);
        return Journeys.beginNewTabGroupUiFlow(mNewTabGroup.scrollToAndSelectTo());
    }

    /** Select "Settings" from the app menu. */
    public SettingsStation openSettings() {
        return mSettings.scrollToAndSelectTo().arriveAt(createSettingsStation());
    }

    /** Select "Select tabs" from the app menu. */
    public TabSwitcherListEditorFacility<HostStationT> clickSelectTabs() {
        return mSelectTabs
                .scrollToAndSelectTo()
                .enterFacility(
                        new TabSwitcherListEditorFacility<>(
                                Collections.emptyList(), Collections.emptyList()));
    }

    /** Select "Delete browsing data" from the app menu. */
    public QuickDeleteDialogFacility clearBrowsingData() {
        assert !mIsIncognito;
        return mQuickDelete.scrollToAndSelectTo().enterFacility(createQuickDeleteDialogFacility());
    }
}
