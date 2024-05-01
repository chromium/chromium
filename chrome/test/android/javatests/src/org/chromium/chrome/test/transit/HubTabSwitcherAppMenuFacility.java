// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;

/** The app menu shown when pressing ("...") in the Hub on a tab switcher pane. */
public class HubTabSwitcherAppMenuFacility extends AppMenuFacility<HubTabSwitcherBaseStation> {

    public static final int SELECT_TABS_ID = R.id.menu_select_tabs;
    public static final ViewElement SELECT_TABS_ITEM = itemElement(SELECT_TABS_ID);

    public HubTabSwitcherAppMenuFacility(HubTabSwitcherBaseStation station) {
        super(station, station.mChromeTabbedActivityTestRule);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareView(NEW_TAB_ITEM);
        elements.declareView(NEW_INCOGNITO_TAB_ITEM);
    }

    /** Clicks "Select tabs" from the app menu. */
    public HubTabSwitcherListEditorFacility clickSelectTabs() {
        recheckActiveConditions();

        HubTabSwitcherListEditorFacility listEditor =
                new HubTabSwitcherListEditorFacility(this.mStation, mChromeTabbedActivityTestRule);

        return StationFacility.enterSync(listEditor, () -> SELECT_TABS_ITEM.perform(click()));
    }
}
