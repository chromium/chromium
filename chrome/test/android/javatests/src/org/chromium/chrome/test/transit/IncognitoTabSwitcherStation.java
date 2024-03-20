// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** The tab switcher screen showing incognito tabs. */
public class IncognitoTabSwitcherStation extends TabSwitcherStation {

    public IncognitoTabSwitcherStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(chromeTabbedActivityTestRule, /* incognito= */ true);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareView(INCOGNITO_TOGGLE_TABS);
        elements.declareView(REGULAR_TOGGLE_TAB_BUTTON);
        elements.declareView(INCOGNITO_TOGGLE_TAB_BUTTON);
    }

    public RegularTabSwitcherStation selectRegularTabList() {
        RegularTabSwitcherStation tabSwitcher =
                new RegularTabSwitcherStation(mChromeTabbedActivityTestRule);
        return Trip.travelSync(this, tabSwitcher, () -> REGULAR_TOGGLE_TAB_BUTTON.perform(click()));
    }
}
