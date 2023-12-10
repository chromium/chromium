// Copyright 2023 The Chromium Authors
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
 * The app menu shown when pressing ("...") in a Tab.
 *
 * <p>TODO(crbug.com/1489724): Actually show the menu in the screen; this just calls
 * #onMenuOrKeyboardAction() at the moment.
 */
public class AppMenuFacility extends StationFacility<BasePageStation> {

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public AppMenuFacility(
            BasePageStation station, ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(station);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        // TODO(crbug.com/1489724): Add menu items as elements to wait for.
    }

    /** Selects "New tab" from the app menu. */
    public NewTabPageStation openNewTab() {
        recheckEnterConditions();

        NewTabPageStation destination =
                new NewTabPageStation(
                        mChromeTabbedActivityTestRule,
                        /* incognito= */ false,
                        /* isOpeningTab= */ true);

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
        recheckEnterConditions();

        NewTabPageStation destination =
                new NewTabPageStation(
                        mChromeTabbedActivityTestRule,
                        /* incognito= */ true,
                        /* isOpeningTab= */ true);

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
}
