// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * The app menu shown when pressing ("...") in the Hub on a tab swicther pane.
 */
public class HubTabSwitcherAppMenuFacility extends StationFacility<HubTabSwitcherBaseStation> {
    // TODO(crbug/1506104): Uncomment once the app menu is hooked up to Hub.
    // public static final Matcher<View> MENU_LIST = withId(R.id.app_menu_list);

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public HubTabSwitcherAppMenuFacility(
            HubTabSwitcherBaseStation station,
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(station);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        // TODO(crbug/1506104): Uncomment once the app menu is hooked up to Hub.
        // elements.declareView(MENU_LIST);
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

        // TODO(crbug/1506104): Uncomment once the app menu is hooked up to Hub.
        // return Trip.travelSync(
        //         mStation,
        //         destination,
        //         (t) -> onView(allOf(isDescendantOfA(MENU_LIST),
        //                       withId(R.id.new_tab_menu_id))));
        return Trip.travelSync(
                mStation,
                destination,
                (t) ->
                        ThreadUtils.postOnUiThread(
                                () ->
                                        mChromeTabbedActivityTestRule
                                                .getActivity()
                                                .onMenuOrKeyboardAction(
                                                        R.id.new_tab_menu_id, true)));
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

        // TODO(crbug/1506104): Uncomment once the app menu is hooked up to Hub.
        // return Trip.travelSync(
        //         mStation,
        //         destination,
        //         (t) -> onView(allOf(isDescendantOfA(MENU_LIST),
        //                       withId(R.id.new_incognito_tab_menu_id))));
        return Trip.travelSync(
                mStation,
                destination,
                (t) ->
                        ThreadUtils.postOnUiThread(
                                () ->
                                        mChromeTabbedActivityTestRule
                                                .getActivity()
                                                .onMenuOrKeyboardAction(
                                                        R.id.new_incognito_tab_menu_id, true)));
    }

    /** Clicks "Select tabs" from the app menu. */
    public HubTabSwitcherListEditorFacility clickSelectTabs() {
        recheckActiveConditions();

        HubTabSwitcherListEditorFacility listEditor =
                new HubTabSwitcherListEditorFacility(this.mStation, mChromeTabbedActivityTestRule);

        // TODO(crbug/1506104): Click menu item directly.
        return StationFacility.enterSync(
                listEditor,
                t1 -> {
                    StationFacility.exitSync(
                            this,
                            t2 -> {
                                ThreadUtils.postOnUiThread(
                                        () ->
                                                mChromeTabbedActivityTestRule
                                                        .getActivity()
                                                        .onMenuOrKeyboardAction(
                                                                R.id.menu_select_tabs, true));
                            });
                });
    }
}
