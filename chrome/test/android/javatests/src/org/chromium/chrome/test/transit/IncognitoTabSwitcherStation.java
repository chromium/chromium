// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.CoreMatchers.allOf;

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

        elements.declareUnownedView(INCOGNITO_TOGGLE_TABS);
        elements.declareUnownedView(REGULAR_TOGGLE_TAB_BUTTON);
        elements.declareUnownedView(INCOGNITO_TOGGLE_TAB_BUTTON);
    }

    public RegularTabSwitcherStation selectRegularTabList() {
        RegularTabSwitcherStation tabSwitcher =
                new RegularTabSwitcherStation(mChromeTabbedActivityTestRule);
        return Trip.travelSync(
                this,
                tabSwitcher,
                (t) -> onView(allOf(isDisplayed(), REGULAR_TOGGLE_TAB_BUTTON)).perform(click()));
    }
}
