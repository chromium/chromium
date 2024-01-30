// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import androidx.annotation.StringRes;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.TravelException;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.R;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** The base station for Hub, with several panes and a toolbar. */
public abstract class HubBaseStation extends TransitStation {
    public static final Matcher<View> HUB_TOOLBAR = withId(R.id.hub_toolbar);
    public static final Matcher<View> HUB_PANE_HOST = withId(R.id.hub_pane_host);
    public static final Matcher<View> HUB_MENU_BUTTON =
            allOf(
                    isDescendantOfA(withId(R.id.hub_toolbar)),
                    withId(org.chromium.chrome.R.id.menu_button));
    public static final Matcher<View> HUB_PANE_SWITCHER =
            allOf(isDescendantOfA(withId(R.id.hub_toolbar)), withId(R.id.pane_switcher));

    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    /**
     * @param chromeTabbedActivityTestRule The {@link ChromeTabbedActivityTestRule} of the test.
     */
    public HubBaseStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super();
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    /** Returns the station's {@link PaneId}. */
    public abstract @PaneId int getPaneId();

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareUnownedView(HUB_TOOLBAR);
        elements.declareUnownedView(HUB_PANE_HOST);
        elements.declareUnownedView(HUB_MENU_BUTTON);

        elements.declareEnterCondition(new HubIsEnabled());
        elements.declareEnterCondition(new HubLayoutShowing());
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
        return Trip.travelSync(this, destination, (t) -> {
          t.addCondition(new HubLayoutNotShowing());
          Espresso.pressBack();
        });
    }

    /**
     * Selects the tab switcher pane on the Hub.
     *
     * @return the corresponding subclass of {@link HubBaseStation}.
     */
    public <T extends HubBaseStation> T selectPane(@PaneId int paneId,
        Class<T> expectedDestination) {
        recheckEnterConditions();

        if (getPaneId() == paneId) {
            return expectedDestination.cast(this);
        }

        T destinationStation = expectedDestination.cast(
            HubStationUtils.createHubStation(paneId, mChromeTabbedActivityTestRule));

        try {
            onView(HUB_PANE_SWITCHER).check(matches(isDisplayed()));
        } catch (NoMatchingViewException e) {
            var throwable = new Throwable(
                "Hub pane switcher is not visible to switch to " + paneId);
            throw new TravelException(this, destinationStation, throwable);
        }

        @StringRes int contentDescriptionId =
            HubStationUtils.getContentDescriptionForIdPaneSelection(paneId);
        return Trip.travelSync(this, destinationStation,
                (t) -> {
                    clickPaneSwitcherForPaneWithContentDescription(contentDescriptionId);
                });
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

    protected class HubLayoutNotShowing extends UiThreadCondition {
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

    private void clickPaneSwitcherForPaneWithContentDescription(
            @StringRes int contentDescriptionRes) {
        // TODO(crbug/1498446): Content description seems reasonable for now, this might get harder
        // once we use a recycler view with text based buttons.
        String contentDescription =
                mChromeTabbedActivityTestRule.getActivity().getString(contentDescriptionRes);
        onView(
                        allOf(
                                isDescendantOfA(HUB_PANE_SWITCHER),
                                withContentDescription(contentDescription)))
                .perform(click());
    }
}
