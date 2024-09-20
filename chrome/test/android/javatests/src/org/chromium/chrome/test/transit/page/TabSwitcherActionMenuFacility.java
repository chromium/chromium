// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;

/** The action menu opened when long pressing the tab switcher button in a {@link PageStation}. */
public class TabSwitcherActionMenuFacility extends Facility<PageStation> {
    public static final ViewSpec APP_MENU_LIST = viewSpec(withId(R.id.app_menu_list));

    // withId() cannot differentiate items because android:id is id/menu_item_text for all items.
    public static final ViewSpec CLOSE_TAB_MENU_ITEM =
            viewSpec(withText(R.string.close_tab), isDescendantOfA(APP_MENU_LIST.getViewMatcher()));

    public static final ViewSpec NEW_TAB_MENU_ITEM =
            viewSpec(
                    withText(R.string.menu_new_tab),
                    isDescendantOfA(APP_MENU_LIST.getViewMatcher()));

    public static final ViewSpec NEW_INCOGNITO_TAB_MENU_ITEM =
            viewSpec(
                    withText(R.string.menu_new_incognito_tab),
                    isDescendantOfA(APP_MENU_LIST.getViewMatcher()));

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(APP_MENU_LIST);
        elements.declareView(CLOSE_TAB_MENU_ITEM);
        elements.declareView(NEW_TAB_MENU_ITEM);
        elements.declareView(NEW_INCOGNITO_TAB_MENU_ITEM);
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on the regular
     * Tab Switcher.
     *
     * <p>This happens when the last regular tab is closed or when the last incognito is closed.
     */
    public RegularTabSwitcherStation selectCloseTabAndDisplayTabSwitcher() {
        TabModelSelector tabModelSelector = mHostStation.getActivity().getTabModelSelector();
        int incognitoTabCount = tabModelSelector.getModel(/* incognito= */ true).getCount();
        int regularTabCount = tabModelSelector.getModel(/* incognito= */ false).getCount();
        if (mHostStation.isIncognito()) {
            incognitoTabCount--;
            assertEquals(
                    "Another incognito tab should be selected instead of entering the tab switcher",
                    0,
                    incognitoTabCount);
        } else {
            regularTabCount--;
        }
        assertEquals(
                "Another regular tab should be selected instead of entering the tab switcher",
                0,
                regularTabCount);

        RegularTabSwitcherStation destination =
                new RegularTabSwitcherStation(/* regularTabsExist= */ false, incognitoTabCount > 0);
        return selectCloseTab(destination);
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on another tab
     * in the same TabModel.
     *
     * <p>This happens when there are other tabs in the same TabModel.
     */
    public <T extends PageStation> T selectCloseTabAndDisplayAnotherTab(
            PageStation.Builder<T> pageStationBuilder) {
        T destination = pageStationBuilder.initFrom(mHostStation).withIsSelectingTabs(1).build();

        return selectCloseTab(destination);
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on a regular
     * tab.
     *
     * <p>This happens when the last incognito tab is closed but there are other regular tabs.
     */
    public <T extends PageStation> T selectCloseTabAndDisplayRegularTab(
            PageStation.Builder<T> pageStationBuilder) {
        T destination =
                pageStationBuilder
                        .withIncognito(false)
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .build();

        return selectCloseTab(destination);
    }

    private <T extends Station> T selectCloseTab(T destination) {
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(
                        createTabCountChangedCondition(mHostStation.isIncognito(), -1)),
                CLOSE_TAB_MENU_ITEM::click);
    }

    /** Select the "New tab" menu option to open a new Tab. */
    public RegularNewTabPageStation selectNewTab() {
        RegularNewTabPageStation destination =
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(
                        createTabCountChangedCondition(/* incognito= */ false, +1)),
                NEW_TAB_MENU_ITEM::click);
    }

    /** Select the "New Incognito tab" menu option to open a new incognito Tab. */
    public IncognitoNewTabPageStation selectNewIncognitoTab() {
        IncognitoNewTabPageStation destination =
                IncognitoNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(
                        createTabCountChangedCondition(/* incognito= */ true, +1)),
                NEW_INCOGNITO_TAB_MENU_ITEM::click);
    }

    private Condition createTabCountChangedCondition(boolean incognito, int change) {
        return new TabCountChangedCondition(
                mHostStation.getActivity().getTabModelSelector().getModel(incognito), change);
    }
}
