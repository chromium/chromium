// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.unscopedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Base class for the screen that shows a webpage with the omnibox and the toolbar.
 *
 * <p>Use the derived {@link PageStation} or {@link EntryPageStation}.
 */
public abstract class BasePageStation extends TransitStation {
    // TODO(crbug.com/1524512): This should be owned, but the tab_switcher_button exists in the
    // tab switcher, even though the tab switcher's toolbar is drawn over it.
    public static final ViewElement TAB_SWITCHER_BUTTON =
            unscopedViewElement(withId(R.id.tab_switcher_button));
    public static final ViewElement MENU_BUTTON =
            unscopedViewElement(withId(R.id.menu_button_wrapper));
    public static final ViewElement MENU_BUTTON2 = unscopedViewElement(withId(R.id.menu_button));

    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    protected final boolean mIncognito;

    protected BasePageStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
        mIncognito = incognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(TAB_SWITCHER_BUTTON);
        elements.declareView(MENU_BUTTON);
        elements.declareView(MENU_BUTTON2);
    }

    /** Long presses the tab switcher button to open the action menu. */
    public TabSwitcherActionMenuFacility openTabSwitcherActionMenu() {
        recheckActiveConditions();

        TabSwitcherActionMenuFacility menu =
                new TabSwitcherActionMenuFacility(this, mChromeTabbedActivityTestRule);
        return StationFacility.enterSync(menu, (e) -> TAB_SWITCHER_BUTTON.perform(longClick()));
    }

    public PageAppMenuFacility openAppMenu() {
        recheckActiveConditions();

        PageAppMenuFacility menu = new PageAppMenuFacility(this, mChromeTabbedActivityTestRule);

        return StationFacility.enterSync(menu, e -> MENU_BUTTON2.perform(click()));
    }

    /** Opens the tab switcher by pressing the toolbar tab switcher button. */
    public <T extends TabSwitcherStation> T openTabSwitcher(Class<T> expectedDestination) {
        recheckActiveConditions();

        T destination;
        if (mIncognito) {
            destination =
                    expectedDestination.cast(
                            new IncognitoTabSwitcherStation(mChromeTabbedActivityTestRule));
        } else {
            destination =
                    expectedDestination.cast(
                            new RegularTabSwitcherStation(mChromeTabbedActivityTestRule));
        }
        return Trip.travelSync(this, destination, (e) -> TAB_SWITCHER_BUTTON.perform(click()));
    }

    /** Opens the hub by pressing the toolbar tab switcher button. */
    public <T extends HubBaseStation> T openHub(Class<T> expectedDestination) {
        recheckActiveConditions();

        T destination =
                expectedDestination.cast(
                        HubStationUtils.createHubStation(
                                mIncognito ? PaneId.INCOGNITO_TAB_SWITCHER : PaneId.TAB_SWITCHER,
                                mChromeTabbedActivityTestRule));

        return Trip.travelSync(this, destination, (e) -> TAB_SWITCHER_BUTTON.perform(click()));
    }

    protected ChromeTabbedActivity getChromeTabbedActivity() {
        ChromeTabbedActivity activity = mChromeTabbedActivityTestRule.getActivity();
        if (activity == null) {
            throw new IllegalStateException("Activity has not yet been created.");
        }
        return activity;
    }
}
