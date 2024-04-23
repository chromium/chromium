// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** The app menu shown when pressing ("...") in the Hub on a tab swicther pane. */
public class HubTabSwitcherAppMenuFacility extends StationFacility<HubTabSwitcherBaseStation> {
    public static final Matcher<View> MENU_LIST = withId(R.id.app_menu_list);
    public static final ViewElement NEW_TAB_ITEM =
            sharedViewElement(allOf(withId(R.id.new_tab_menu_id), isDescendantOfA(MENU_LIST)));
    public static final ViewElement NEW_INCOGNITO_TAB_ITEM =
            sharedViewElement(
                    allOf(withId(R.id.new_incognito_tab_menu_id), isDescendantOfA(MENU_LIST)));
    public static final ViewElement CLOSE_ALL_REGULAR_TABS_ITEM =
            sharedViewElement(
                    allOf(withId(R.id.close_all_tabs_menu_id), isDescendantOfA(MENU_LIST)));
    public static final ViewElement CLOSE_ALL_INCOGNITO_TABS_MENU_ITEM =
            sharedViewElement(
                    allOf(
                            withId(R.id.close_all_incognito_tabs_menu_id),
                            isDescendantOfA(MENU_LIST)));
    public static final ViewElement SELECT_TABS_ITEM =
            sharedViewElement(allOf(withId(R.id.menu_select_tabs), isDescendantOfA(MENU_LIST)));
    public static final ViewElement CLEAR_BROWSING_DATA_ITEM =
            sharedViewElement(allOf(withId(R.id.quick_delete_menu_id), isDescendantOfA(MENU_LIST)));
    public static final ViewElement SETTINGS_ITEM =
            sharedViewElement(allOf(withId(R.id.preferences_id), isDescendantOfA(MENU_LIST)));

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    private final boolean mIsIncognito;

    public HubTabSwitcherAppMenuFacility(
            HubTabSwitcherBaseStation station,
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule,
            boolean isIncognito) {
        super(station);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
        mIsIncognito = isIncognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        if (!mIsIncognito) {
            elements.declareView(NEW_TAB_ITEM);
            elements.declareView(NEW_INCOGNITO_TAB_ITEM);
            elements.declareView(CLOSE_ALL_REGULAR_TABS_ITEM);
            elements.declareView(SELECT_TABS_ITEM);
            elements.declareView(CLEAR_BROWSING_DATA_ITEM);
            elements.declareView(SETTINGS_ITEM);
        } else {
            elements.declareView(NEW_TAB_ITEM);
            elements.declareView(NEW_INCOGNITO_TAB_ITEM);
            elements.declareView(CLOSE_ALL_INCOGNITO_TABS_MENU_ITEM);
            elements.declareView(SELECT_TABS_ITEM);
            elements.declareView(SETTINGS_ITEM);
        }
    }

    /** Selects "New tab" from the app menu. */
    public NewTabPageStation openNewTab() {
        recheckActiveConditions();

        NewTabPageStation destination =
                NewTabPageStation.newBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();

        return Trip.travelSync(mStation, destination, () -> NEW_TAB_ITEM.perform(click()));
    }

    /** Selects "New Incognito tab" from the app menu. */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        recheckActiveConditions();

        IncognitoNewTabPageStation destination =
                IncognitoNewTabPageStation.newBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();

        return Trip.travelSync(
                mStation, destination, () -> NEW_INCOGNITO_TAB_ITEM.perform(click()));
    }

    /** Clicks "Select tabs" from the app menu. */
    public HubTabSwitcherListEditorFacility clickSelectTabs() {
        recheckActiveConditions();

        HubTabSwitcherListEditorFacility listEditor =
                new HubTabSwitcherListEditorFacility(this.mStation, mChromeTabbedActivityTestRule);

        return StationFacility.enterSync(listEditor, () -> SELECT_TABS_ITEM.perform(click()));
    }
}
