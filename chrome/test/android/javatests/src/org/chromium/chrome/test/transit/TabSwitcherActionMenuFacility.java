// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;
import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** The action menu opened when long pressing the tab switcher button in a {@link PageStation}. */
public class TabSwitcherActionMenuFacility extends StationFacility<PageStation> {
    public static final ViewElement APP_MENU_LIST = sharedViewElement(withId(R.id.app_menu_list));
    // withId() cannot differentiate items because android:id is id/menu_item_text for all items.
    public static final ViewElement CLOSE_TAB_MENU_ITEM =
            scopedViewElement(
                    allOf(
                            withText(R.string.close_tab),
                            isDescendantOfA(APP_MENU_LIST.getViewMatcher())));
    public static final ViewElement NEW_TAB_MENU_ITEM =
            scopedViewElement(
                    allOf(
                            withText(R.string.menu_new_tab),
                            isDescendantOfA(APP_MENU_LIST.getViewMatcher())));
    public static final ViewElement NEW_INCOGNITO_TAB_MENU_ITEM =
            scopedViewElement(
                    allOf(
                            withText(R.string.menu_new_incognito_tab),
                            isDescendantOfA(APP_MENU_LIST.getViewMatcher())));

    public TabSwitcherActionMenuFacility(PageStation pageStation) {
        super(pageStation);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(APP_MENU_LIST);
        elements.declareView(CLOSE_TAB_MENU_ITEM);
        elements.declareView(NEW_TAB_MENU_ITEM);
        elements.declareView(NEW_INCOGNITO_TAB_MENU_ITEM);
    }

    /** Select the "Close tab" menu option to close the current Tab. */
    public <T extends TransitStation> T selectCloseTab(Class<T> expectedDestination) {
        T destination;
        TabModelSelector tabModelSelector =
                mStation.getTestRule().getActivity().getTabModelSelector();
        if (tabModelSelector.getCurrentModel().getCount() <= 1) {
            if (tabModelSelector.isIncognitoSelected()) {
                // No tabs left, so closing the last will either take us to a normal tab, or the tab
                // switcher if no normal tabs exist.
                if (tabModelSelector.getModel(false).getCount() == 0) {
                    if (HubFieldTrial.isHubEnabled()) {
                        destination =
                                expectedDestination.cast(
                                        new HubTabSwitcherStation(mStation.getTestRule()));
                    } else {
                        destination =
                                expectedDestination.cast(
                                        new RegularTabSwitcherStation(mStation.getTestRule()));
                    }
                } else {
                    destination =
                            expectedDestination.cast(
                                    PageStation.newPageStationBuilder()
                                            .withActivityTestRule(mStation.getTestRule())
                                            .withIncognito(false)
                                            .withIsOpeningTab(false)
                                            .withIsSelectingTab(true)
                                            .build());
                }
            } else {
                // No tabs left, so closing the last will take us to the tab switcher.
                if (HubFieldTrial.isHubEnabled()) {
                    destination =
                            expectedDestination.cast(
                                    new HubTabSwitcherStation(mStation.getTestRule()));
                } else {
                    destination =
                            expectedDestination.cast(
                                    new RegularTabSwitcherStation(mStation.getTestRule()));
                }
            }
        } else {
            // Another tab will be displayed.
            destination =
                    expectedDestination.cast(
                            PageStation.newPageStationBuilder()
                                    .withActivityTestRule(mStation.getTestRule())
                                    .withIncognito(tabModelSelector.isIncognitoSelected())
                                    .withIsOpeningTab(false)
                                    .withIsSelectingTab(true)
                                    .build());
        }

        return Trip.travelSync(mStation, destination, () -> CLOSE_TAB_MENU_ITEM.perform(click()));
    }

    /** Select the "New tab" menu option to open a new Tab. */
    public PageStation selectNewTab() {
        PageStation destination =
                PageStation.newPageStationBuilder()
                        .withActivityTestRule(mStation.getTestRule())
                        .withIncognito(false)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();
        return Trip.travelSync(mStation, destination, () -> NEW_TAB_MENU_ITEM.perform(click()));
    }

    /** Select the "New Incognito tab" menu option to open a new incognito Tab. */
    public PageStation selectNewIncognitoTab() {
        PageStation destination =
                PageStation.newPageStationBuilder()
                        .withActivityTestRule(mStation.getTestRule())
                        .withIncognito(true)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();
        return Trip.travelSync(
                mStation, destination, () -> NEW_INCOGNITO_TAB_MENU_ITEM.perform(click()));
    }
}
