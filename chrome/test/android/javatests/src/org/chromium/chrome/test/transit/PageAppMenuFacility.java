// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.annotation.CallSuper;

import org.chromium.base.test.transit.Elements;

/**
 * The app menu shown when pressing ("...") in a Tab.
 *
 * <p>Use subclasses to access menu items not shared between all PageStation types:
 *
 * <ul>
 *   <li>{@link NewTabPageRegularAppMenuFacility}
 *   <li>{@link NewTabPageIncognitoAppMenuFacility}
 *   <li>{@link WebPageRegularAppMenuFacility}
 *   <li>{@link WebPageIncognitoAppMenuFacility}
 * </ul>
 *
 * @param <HostPageStationT> the type of host {@link PageStation} where this app menu is opened.
 */
public class PageAppMenuFacility<HostPageStationT extends PageStation>
        extends AppMenuFacility<HostPageStationT> {

    protected Item<NewTabPageStation> mNewTab;
    protected Item<IncognitoNewTabPageStation> mNewIncognitoTab;
    protected Item<SettingsStation> mSettings;

    public PageAppMenuFacility(HostPageStationT station) {
        super(station);
    }

    @Override
    @CallSuper
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        // TODO: Declare top buttons (forward, reload, bookmark, etc.).
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        // TODO: Declare more common menu items

        mNewTab = declareMenuItemToStation(items, NEW_TAB_ID, this::createNewTabPageStation);
        mNewIncognitoTab =
                declareMenuItemToStation(
                        items, NEW_INCOGNITO_TAB_ID, this::createIncognitoNewTabPageStation);
        mSettings = declareMenuItemToStation(items, SETTINGS_ID, this::createSettingsStation);
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
