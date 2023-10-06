// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * The action menu opened when long pressing the tab switcher button in a {@link BasePageStation}.
 */
public class TabSwitcherActionMenuFacility extends StationFacility<BasePageStation> {
    public static final Matcher<View> APP_MENU_LIST = withId(R.id.app_menu_list);
    // withId() cannot differentiate items because android:id is id/menu_item_text for all items.
    public static final Matcher<View> CLOSE_TAB_MENU_ITEM =
            allOf(withText(R.string.close_tab), isDescendantOfA(APP_MENU_LIST));
    public static final Matcher<View> NEW_TAB_MENU_ITEM =
            allOf(withText(R.string.menu_new_tab), isDescendantOfA(APP_MENU_LIST));
    public static final Matcher<View> NEW_INCOGNITO_TAB_MENU_ITEM =
            allOf(withText(R.string.menu_new_incognito_tab), isDescendantOfA(APP_MENU_LIST));
    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public TabSwitcherActionMenuFacility(
            BasePageStation pageStation,
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(pageStation);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
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
                mChromeTabbedActivityTestRule.getActivity().getTabModelSelector();
        if (tabModelSelector.getCurrentModel().getCount() <= 1) {
            if (tabModelSelector.isIncognitoSelected()) {
                // No tabs left, so closing the last will either take us to a normal tab, or the tab
                // switcher if no normal tabs exist.
                if (tabModelSelector.getModel(false).getCount() == 0) {
                    destination =
                            expectedDestination.cast(
                                    new TabSwitcherStation(mChromeTabbedActivityTestRule));
                } else {
                    destination =
                            expectedDestination.cast(
                                    new PageStation(
                                            mChromeTabbedActivityTestRule,
                                            /*incognito*/ false, /*isOpeningTab*/
                                            false));
                }
            } else {
                // No tabs left, so closing the last will take us to the tab switcher.
                destination =
                        expectedDestination.cast(
                                new TabSwitcherStation(mChromeTabbedActivityTestRule));
            }
        } else {
            // Another tab will be displayed.
            destination =
                    expectedDestination.cast(
                            new PageStation(
                                    mChromeTabbedActivityTestRule,
                                    tabModelSelector.isIncognitoSelected(), /*isOpeningTab*/
                                    false));
        }

        return Trip.goSync(
                mStation, destination, (t) -> onView(CLOSE_TAB_MENU_ITEM).perform(click()));
    }

    /** Select the "New tab" menu option to open a new Tab. */
    public PageStation selectNewTab() {
        PageStation destination =
                new PageStation(
                        mChromeTabbedActivityTestRule, /*incognito*/ false, /*isOpeningTab*/ true);
        return Trip.goSync(
                mStation, destination, (t) -> onView(NEW_TAB_MENU_ITEM).perform(click()));
    }

    /** Select the "New Incognito tab" menu option to open a new incognito Tab. */
    public PageStation selectNewIncognitoTab() {
        PageStation destination =
                new PageStation(
                        mChromeTabbedActivityTestRule, /*incognito*/ true, /*isOpeningTab*/ true);
        return Trip.goSync(
                mStation, destination, (t) -> onView(NEW_INCOGNITO_TAB_MENU_ITEM).perform(click()));
    }
}
