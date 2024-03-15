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

import static org.chromium.base.test.transit.LogicalElement.sharedUiThreadLogicalElement;
import static org.chromium.base.test.transit.LogicalElement.unscopedUiThreadLogicalElement;
import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import androidx.annotation.StringRes;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.TravelException;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.R;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** The base station for Hub, with several panes and a toolbar. */
public abstract class HubBaseStation extends TransitStation {
    public static final ViewElement HUB_TOOLBAR = sharedViewElement(withId(R.id.hub_toolbar));
    public static final ViewElement HUB_PANE_HOST = sharedViewElement(withId(R.id.hub_pane_host));
    public static final ViewElement HUB_MENU_BUTTON =
            sharedViewElement(
                    allOf(
                            isDescendantOfA(withId(R.id.hub_toolbar)),
                            withId(org.chromium.chrome.R.id.menu_button)));
    public static final ViewElement HUB_PANE_SWITCHER =
            sharedViewElement(
                    allOf(isDescendantOfA(withId(R.id.hub_toolbar)), withId(R.id.pane_switcher)));

    public static final ViewElement REGULAR_TOGGLE_TAB_BUTTON =
            sharedViewElement(
                    allOf(
                            withContentDescription(
                                    R.string.accessibility_tab_switcher_standard_stack)));

    public static final ViewElement INCOGNITO_TOGGLE_TAB_BUTTON =
            sharedViewElement(
                    allOf(
                            withContentDescription(
                                    R.string.accessibility_tab_switcher_incognito_stack)));

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
        elements.declareView(HUB_TOOLBAR);
        elements.declareView(HUB_PANE_HOST);
        elements.declareView(HUB_MENU_BUTTON);

        Condition incognitoTabsExist =
                new UiThreadCondition() {
                    @Override
                    public boolean check() {
                        return mChromeTabbedActivityTestRule.tabsCount(/* incognito= */ true) > 0;
                    }

                    @Override
                    public String buildDescription() {
                        return "Incognito tabs exist";
                    }
                };

        elements.declareViewIf(REGULAR_TOGGLE_TAB_BUTTON, incognitoTabsExist);
        elements.declareViewIf(INCOGNITO_TOGGLE_TAB_BUTTON, incognitoTabsExist);

        elements.declareLogicalElement(
                unscopedUiThreadLogicalElement(
                        "HubFieldTrial Hub is enabled", HubFieldTrial::isHubEnabled));
        elements.declareLogicalElement(
                sharedUiThreadLogicalElement(
                        "LayoutManager is showing TAB_SWITCHER (Hub)", this::isHubLayoutShowing));
        elements.declareLogicalElement(
                unscopedUiThreadLogicalElement(
                        "LayoutManager is not in transition to or from TAB_SWITCHER (Hub)",
                        this::isHubLayoutNotInTransition));
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
                        /* isOpeningTab= */ false,
                        /* isSelectingTab= */ true);
        return Trip.travelSync(this, destination, (t) -> Espresso.pressBack());
    }

    /**
     * Selects the tab switcher pane on the Hub.
     *
     * @return the corresponding subclass of {@link HubBaseStation}.
     */
    public <T extends HubBaseStation> T selectPane(@PaneId int paneId,
        Class<T> expectedDestination) {
        recheckActiveConditions();

        if (getPaneId() == paneId) {
            return expectedDestination.cast(this);
        }

        T destinationStation = expectedDestination.cast(
            HubStationUtils.createHubStation(paneId, mChromeTabbedActivityTestRule));

        try {
            HUB_PANE_SWITCHER.onView().check(matches(isDisplayed()));
        } catch (NoMatchingViewException e) {
            var throwable = new Throwable(
                "Hub pane switcher is not visible to switch to " + paneId);
            throw TravelException.newTripException(this, destinationStation, throwable);
        }

        @StringRes int contentDescriptionId =
            HubStationUtils.getContentDescriptionForIdPaneSelection(paneId);
        return Trip.travelSync(this, destinationStation,
                (t) -> {
                    clickPaneSwitcherForPaneWithContentDescription(contentDescriptionId);
                });
    }

    /** Convenience method to select the Regular Tab Switcher pane. */
    public HubTabSwitcherStation selectRegularTabList() {
        return selectPane(PaneId.TAB_SWITCHER, HubTabSwitcherStation.class);
    }

    /** Convenience method to select the Incognito Tab Switcher pane. */
    public HubIncognitoTabSwitcherStation selectIncognitoTabList() {
        return selectPane(PaneId.INCOGNITO_TAB_SWITCHER, HubIncognitoTabSwitcherStation.class);
    }

    private boolean isHubLayoutShowing() {
        LayoutManager layoutManager =
                mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
        return layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
    }

    private boolean isHubLayoutNotInTransition() {
        LayoutManager layoutManager =
                mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
        return !layoutManager.isLayoutStartingToShow(LayoutType.TAB_SWITCHER)
                && !layoutManager.isLayoutStartingToHide(LayoutType.TAB_SWITCHER);
    }

    private void clickPaneSwitcherForPaneWithContentDescription(
            @StringRes int contentDescriptionRes) {
        // TODO(crbug/1498446): Content description seems reasonable for now, this might get harder
        // once we use a recycler view with text based buttons.
        String contentDescription =
                mChromeTabbedActivityTestRule.getActivity().getString(contentDescriptionRes);
        onView(
                        allOf(
                                isDescendantOfA(HUB_PANE_SWITCHER.getViewMatcher()),
                                withContentDescription(contentDescription)))
                .perform(click());
    }
}
