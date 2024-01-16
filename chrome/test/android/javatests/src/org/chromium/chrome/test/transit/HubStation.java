// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import androidx.test.espresso.Espresso;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.hub.R;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** The Hub screen, with several panes and a toolbar. */
public class HubStation extends TransitStation {
    public static final Matcher<View> HUB_TOOLBAR = withId(R.id.hub_toolbar);
    public static final Matcher<View> HUB_PANE_HOST = withId(R.id.hub_pane_host);
    public static final Matcher<View> HUB_MENU_BUTTON =
            allOf(
                    isDescendantOfA(withId(R.id.hub_toolbar)),
                    withId(org.chromium.chrome.R.id.menu_button));

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    /**
     * @param chromeTabbedActivityTestRule The {@link ChromeTabbedActivityTestRule} of the test.
     */
    public HubStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super();
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(HUB_TOOLBAR);
        elements.declareView(HUB_PANE_HOST);
        elements.declareView(HUB_MENU_BUTTON);

        elements.declareEnterCondition(new HubIsEnabled());
        elements.declareEnterCondition(new HubLayoutShowing());
        elements.declareExitCondition(new HubLayoutNotShowing());
    }

    /**
     * Returns to the previous tab via the back button.
     *
     * @return the {@link PageStation} that Hub returned to.
     */
    public PageStation leaveHubToPreviousTabViaBack() {
        // TODO(crbug/1498446): This logic gets exponentially more complicated if there is
        // additional back state e.g. in-pane navigations, between pane navigations, etc. Figure out
        // a solution that better handles the complexity.
        PageStation destination =
                new PageStation(
                        mChromeTabbedActivityTestRule,
                        /* incognito= */ false,
                        /* isOpeningTab= */ false);
        return Trip.travelSync(this, destination, (t) -> Espresso.pressBack());
    }

    /**
     * Opens the app menu on the current pane.
     *
     * @return the {@link HubAppMenuFacility} for the Hub.
     */
    public HubAppMenuFacility openAppMenu() {
        recheckEnterConditions();

        HubAppMenuFacility menu = new HubAppMenuFacility(this, mChromeTabbedActivityTestRule);

        return StationFacility.enterSync(menu, (e) -> onView(HUB_MENU_BUTTON).perform(click()));
    }

    private class HubIsEnabled extends UiThreadCondition {
        @Override
        public boolean check() {
            return HubFieldTrial.isHubEnabled();
        }

        @Override
        public String buildDescription() {
            return "HubFieldTrial Hub is enabled";
        }
    }

    private class HubLayoutShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            return layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)
                    && !layoutManager.isLayoutStartingToShow(LayoutType.TAB_SWITCHER)
                    && !layoutManager.isLayoutStartingToHide(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is showing TAB_SWITCHER (Hub)";
        }
    }

    private class HubLayoutNotShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            return !layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is not showing TAB_SWITCHER (Hub)";
        }
    }
}
