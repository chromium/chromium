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

/** The app menu shown when pressing ("...") in a Tab. */
public class PageAppMenuFacility extends StationFacility<PageStation> {
    public static final ViewElement NEW_TAB_MENU_ITEM =
            ViewElement.sharedViewElement(withId(R.id.new_tab_menu_id));
    public static final ViewElement NEW_INCOGNITO_TAB_MENU_ITEM =
            ViewElement.sharedViewElement(withId(R.id.new_incognito_tab_menu_id));

    public PageAppMenuFacility(PageStation station) {
        super(station);
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
                NewTabPageStation.newBuilder()
                        .initFrom(mStation)
                        .withIncognito(false)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();

        return Trip.travelSync(mStation, destination, () -> NEW_TAB_MENU_ITEM.perform(click()));
    }

    /** Selects "New Incognito tab" from the app menu. */
    public NewTabPageStation openNewIncognitoTab() {
        recheckActiveConditions();

        NewTabPageStation destination =
                NewTabPageStation.newBuilder()
                        .initFrom(mStation)
                        .withIncognito(true)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();

        return Trip.travelSync(
                mStation, destination, () -> NEW_INCOGNITO_TAB_MENU_ITEM.perform(click()));
    }
}
