// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.junit.Assert.assertNotNull;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
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
 * <p>Use subclasses to access menu items not shared between all {@link CtaPageStation} types:
 *
 * <ul>
 *   <li>{@link RegularNewTabPageAppMenuFacility}
 *   <li>{@link IncognitoNewTabPageAppMenuFacility}
 *   <li>{@link RegularWebPageAppMenuFacility}
 *   <li>{@link IncognitoWebPageAppMenuFacility}
 * </ul>
 *
 * @param <HostPageStationT> the type of host {@link CtaPageStation} where this app menu is opened.
 */
public class PageAppMenuFacility<HostPageStationT extends CtaPageStation>
        extends CtaAppMenuFacility<HostPageStationT> {

    protected Item mNewTab;
    protected Item mNewIncognitoTab;
    protected @Nullable Item mAddToGroup;
    protected Item mNewWindow;
    protected Item mSettings;
    protected Item mPinTab;
    protected Item mUnpinTab;

    @Override
    protected void declareItems(ItemsBuilder items) {
        // TODO: Declare top buttons (forward, reload, bookmark, etc.).
        // TODO: Declare more common menu items

        mNewTab = declareMenuItem(items, NEW_TAB_ID);
        mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);
        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            mAddToGroup = declareMenuItem(items, ADD_TO_GROUP_ID);
        }
        if (ChromeFeatureList.sAndroidPinnedTabs.isEnabled()) {
            // At most one of these exist.
            mPinTab = declarePossibleMenuItem(items, PIN_TAB);
            mUnpinTab = declarePossibleMenuItem(items, UNPIN_TAB);
        }
        mSettings = declareMenuItem(items, SETTINGS_ID);

    }

    /** Select "New tab" from the app menu. */
    public RegularNewTabPageStation openNewTab() {
        return mNewTab.scrollToAndSelectTo().arriveAt(createNewTabPageStation());
    }

    /** Select "New Incognito tab" from the app menu. */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        return mNewIncognitoTab.scrollToAndSelectTo().arriveAt(createIncognitoNewTabPageStation());
    }

    /** Select "New window" from the app menu. */
    public RegularNewTabPageStation openNewWindow() {
        TabbedAppMenuPropertiesDelegate delegate = getTabbedAppMenuPropertiesDelegate();
        assert delegate.shouldShowNewWindow() : "App menu is not expected to show 'New window'";
        return mNewWindow.scrollToAndSelectTo().inNewTask().arriveAt(createNewWindowStation());
    }

    /**
     * Select "Add to group" from the app menu. This opens a bottom sheet to add the current tab to
     * a tab group.
     */
    public TabGroupListBottomSheetFacility<HostPageStationT> selectAddToGroupWithBottomSheet() {
        assertNotNull(mAddToGroup);

        TabGroupModelFilter tabGroupModelFilter = mHostStation.getTabGroupModelFilter();
        Set<Token> tabGroupIds = tabGroupModelFilter.getAllTabGroupIds();
        return mAddToGroup
                .scrollToAndSelectTo()
                .enterFacility(
                        new TabGroupListBottomSheetFacility<>(
                                new ArrayList<>(tabGroupIds), /* isNewTabGroupRowVisible= */ true));
    }

    /** Select "Pin tab" from the app menu. */
    public void pinTab() {
         mPinTab.scrollToAndSelectTo().complete();
    }

    /** Select "Unpin tab" from the app menu. */
    public void unpinTab() {
         mUnpinTab.scrollToAndSelectTo().complete();
    }


    private TabbedAppMenuPropertiesDelegate getTabbedAppMenuPropertiesDelegate() {
        return (TabbedAppMenuPropertiesDelegate)
                mHostStation
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getAppMenuCoordinatorForTesting()
                        .getAppMenuPropertiesDelegate();
    }

    /** Select "Settings" from the app menu. */
    public SettingsStation openSettings() {
        return mSettings.scrollToAndSelectTo().arriveAt(createSettingsStation());
    }
}
