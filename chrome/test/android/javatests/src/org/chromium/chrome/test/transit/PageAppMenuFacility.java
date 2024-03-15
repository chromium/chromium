// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * The app menu shown when pressing ("...") in a Tab.
 */
public class PageAppMenuFacility extends StationFacility<BasePageStation> {
    public static final ViewElement NEW_TAB_MENU_ITEM =
            ViewElement.sharedViewElement(withId(R.id.new_tab_menu_id));
    public static final ViewElement NEW_INCOGNITO_TAB_MENU_ITEM =
            ViewElement.sharedViewElement(withId(R.id.new_incognito_tab_menu_id));

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public PageAppMenuFacility(
            BasePageStation station, ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(station);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(NEW_TAB_MENU_ITEM);
        elements.declareView(NEW_INCOGNITO_TAB_MENU_ITEM);
    }

    /** Selects "New tab" from the app menu. */
    public NewTabPageStation openNewTab() {
        recheckActiveConditions();

        NewTabPageStation destination =
                new NewTabPageStation(
                        mChromeTabbedActivityTestRule,
                        /* incognito= */ false,
                        /* isOpeningTab= */ true,
                        /* isSelectingTab= */ true);

        return Trip.travelSync(mStation, destination, t -> NEW_TAB_MENU_ITEM.perform(click()));
    }

    /** Selects "New Incognito tab" from the app menu. */
    public NewTabPageStation openNewIncognitoTab() {
        recheckActiveConditions();

        NewTabPageStation destination =
                new NewTabPageStation(
                        mChromeTabbedActivityTestRule,
                        /* incognito= */ true,
                        /* isOpeningTab= */ true,
                        /* isSelectingTab= */ true);

        return Trip.travelSync(
                mStation, destination, t -> NEW_INCOGNITO_TAB_MENU_ITEM.perform(click()));
    }
}
