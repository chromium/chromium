// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.junit.Assert.assertNotNull;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.test.transit.Station;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabbed_mode.TabbedAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.test.transit.CtaAppMenuFacility;
import org.chromium.chrome.test.transit.hub.TabGroupListBottomSheetFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;

import java.util.ArrayList;
import java.util.Set;

/**
 * The app menu shown when pressing ("...") in a Tab.
 *
 * <p>Use subclasses to access menu items not shared between all PageStation types:
 *
 * <ul>
 *   <li>{@link RegularNewTabPageAppMenuFacility}
 *   <li>{@link IncognitoNewTabPageAppMenuFacility}
 *   <li>{@link RegularWebPageAppMenuFacility}
 *   <li>{@link IncognitoWebPageAppMenuFacility}
 * </ul>
 *
 * @param <HostPageStationT> the type of host {@link PageStation} where this app menu is opened.
 */
public class PageAppMenuFacility<HostPageStationT extends PageStation>
        extends CtaAppMenuFacility<HostPageStationT> {

    protected Item<RegularNewTabPageStation> mNewTab;
    protected Item<IncognitoNewTabPageStation> mNewIncognitoTab;
    protected @Nullable Item<TabGroupListBottomSheetFacility<HostPageStationT>> mAddToGroup;
    protected Item<RegularNewTabPageStation> mNewWindow;
    protected Item<SettingsStation> mSettings;

    @Override
    protected void declareItems(ItemsBuilder items) {
        // TODO: Declare top buttons (forward, reload, bookmark, etc.).
        // TODO: Declare more common menu items

        mNewTab = declareMenuItemToStation(items, NEW_TAB_ID, this::createNewTabPageStation);
        mNewIncognitoTab =
                declareMenuItemToStation(
                        items, NEW_INCOGNITO_TAB_ID, this::createIncognitoNewTabPageStation);
        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            mAddToGroup =
                    declareMenuItemToFacility(
                            items, ADD_TO_GROUP_ID, this::createTabGroupListBottomSheetFacility);
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

    /** Select "New window" from the app menu. */
    public RegularNewTabPageStation openNewWindow() {
        TabbedAppMenuPropertiesDelegate delegate = getTabbedAppMenuPropertiesDelegate();
        assert delegate.shouldShowNewWindow() : "App menu is not expected to show 'New window'";
        return mNewWindow.scrollToAndSelect();
    }

    /**
     * Select "Add to group" from the app menu. This opens a bottom sheet to add the current tab to
     * a tab group.
     */
    public TabGroupListBottomSheetFacility<HostPageStationT> selectAddToGroupWithBottomSheet() {
        assertNotNull(mAddToGroup);
        return mAddToGroup.scrollToAndSelect();
    }

    private TabbedAppMenuPropertiesDelegate getTabbedAppMenuPropertiesDelegate() {
        return (TabbedAppMenuPropertiesDelegate)
                mHostStation
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getAppMenuCoordinatorForTesting()
                        .getAppMenuPropertiesDelegate();
    }

    protected TabGroupListBottomSheetFacility<HostPageStationT>
            createTabGroupListBottomSheetFacility() {
        TabGroupModelFilter tabGroupModelFilter =
                mHostStation
                        .getActivity()
                        .getTabModelSelector()
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilter();
        Set<Token> tabGroupIds = tabGroupModelFilter.getAllTabGroupIds();
        return new TabGroupListBottomSheetFacility<>(
                new ArrayList<>(tabGroupIds), /* isNewTabGroupRowVisible= */ true);
    }

    /** Select "Settings" from the app menu. */
    public SettingsStation openSettings() {
        return mSettings.scrollToAndSelect();
    }

    /**
     * Use as lambda from subclasses to handle selecting |mNewWindow|.
     *
     * <p>Called from {@link #openNewWindow()} after scrolling to the item.
     */
    protected RegularNewTabPageStation handleOpenNewWindow(
            ItemOnScreenFacility<RegularNewTabPageStation> itemOnScreen) {
        return Station.spawnSync(
                createNewWindowStation(), itemOnScreen.viewElement.getClickTrigger());
    }
}
