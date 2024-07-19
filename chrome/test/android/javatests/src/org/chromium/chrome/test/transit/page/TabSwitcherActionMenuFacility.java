// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;

/** The action menu opened when long pressing the tab switcher button in a {@link PageStation}. */
public class TabSwitcherActionMenuFacility extends Facility<PageStation> {
    public static final ViewElement APP_MENU_LIST = scopedViewElement(withId(R.id.app_menu_list));
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

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(APP_MENU_LIST);
        elements.declareView(CLOSE_TAB_MENU_ITEM);
        elements.declareView(NEW_TAB_MENU_ITEM);
        elements.declareView(NEW_INCOGNITO_TAB_MENU_ITEM);
    }

    /** Select the "Close tab" menu option to close the current Tab. */
    public <T extends Station> T selectCloseTab(Class<T> expectedDestination) {
        T destination;
        TabModelSelector tabModelSelector = mHostStation.getActivity().getTabModelSelector();
        int incognitoTabCount = tabModelSelector.getModel(/* incognito= */ true).getCount();
        int regularTabCount = tabModelSelector.getModel(/* incognito= */ false).getCount();
        if (tabModelSelector.getCurrentModel().getCount() <= 1) {
            if (tabModelSelector.isIncognitoSelected()) {
                // No tabs left, so closing the last will either take us to a normal tab, or the tab
                // switcher if no normal tabs exist.
                if (tabModelSelector.getModel(/* incognito= */ false).getCount() == 0) {
                    destination =
                            expectedDestination.cast(
                                    new RegularTabSwitcherStation(
                                            regularTabCount > 0, /* incognitoTabsExist= */ false));
                } else {
                    destination =
                            expectedDestination.cast(
                                    PageStation.newPageStationBuilder()
                                            .withIncognito(false)
                                            .withIsOpeningTabs(0)
                                            .withIsSelectingTabs(1)
                                            .build());
                }
            } else {
                // No tabs left, so closing the last will take us to the tab switcher.
                destination =
                        expectedDestination.cast(
                                new RegularTabSwitcherStation(
                                        /* regularTabsExist= */ false, incognitoTabCount > 0));
            }
        } else {
            // Another tab will be displayed.
            destination =
                    expectedDestination.cast(
                            PageStation.newPageStationBuilder()
                                    .withIncognito(tabModelSelector.isIncognitoSelected())
                                    .withIsOpeningTabs(0)
                                    .withIsSelectingTabs(1)
                                    .build());
        }

        return mHostStation.travelToSync(destination, () -> CLOSE_TAB_MENU_ITEM.perform(click()));
    }

    /** Select the "New tab" menu option to open a new Tab. */
    public RegularNewTabPageStation selectNewTab() {
        RegularNewTabPageStation destination =
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(destination, () -> NEW_TAB_MENU_ITEM.perform(click()));
    }

    /** Select the "New Incognito tab" menu option to open a new incognito Tab. */
    public IncognitoNewTabPageStation selectNewIncognitoTab() {
        IncognitoNewTabPageStation destination =
                IncognitoNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(
                destination, () -> NEW_INCOGNITO_TAB_MENU_ITEM.perform(click()));
    }
}
