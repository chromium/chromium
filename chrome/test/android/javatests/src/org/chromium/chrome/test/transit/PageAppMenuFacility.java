// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.Elements;

import java.util.List;

/** The app menu shown when pressing ("...") in a Tab. */
public class PageAppMenuFacility extends AppMenuFacility<PageStation> {

    private Item<NewTabPageStation> mNewTab;
    private Item<IncognitoNewTabPageStation> mNewIncognitoTab;
    private Item<SettingsStation> mSettings;

    public PageAppMenuFacility(PageStation station) {
        super(station, station.mChromeTabbedActivityTestRule);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        // TODO: Declare top buttons (forward, reload, bookmark, etc.).
    }

    @Override
    protected void declareItems(List<Item<?>> items) {
        // TODO: Declare more menu items

        mNewTab = newMenuItemToStation(NEW_TAB_ID, this::createNewTabPageStation);
        mNewIncognitoTab =
                newMenuItemToStation(NEW_INCOGNITO_TAB_ID, this::createIncognitoNewTabPageStation);
        mSettings = newMenuItemToStation(SETTINGS_ID, this::createSettingsStation);

        items.add(mNewTab);
        items.add(mNewIncognitoTab);
        items.add(mSettings);
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
}
