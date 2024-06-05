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

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.LogicalElement.uiThreadLogicalElement;
import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import androidx.annotation.StringRes;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ActivityElement;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.TravelException;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.R;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** The base station for Hub, with several panes and a toolbar. */
public abstract class HubBaseStation extends Station {
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

    protected ActivityElement<ChromeTabbedActivity> mActivityElement;
    protected TabModelSelectorCondition mTabModelSelectorCondition;

    public HubBaseStation() {
        super();
        assert HubFieldTrial.isHubEnabled();
    }

    /** Returns the station's {@link PaneId}. */
    public abstract @PaneId int getPaneId();

    @Override
    public void declareElements(Elements.Builder elements) {
        mActivityElement = elements.declareActivity(ChromeTabbedActivity.class);
        mTabModelSelectorCondition =
                elements.declareEnterCondition(new TabModelSelectorCondition(mActivityElement));

        elements.declareView(HUB_TOOLBAR);
        elements.declareView(HUB_PANE_HOST);
        elements.declareView(HUB_MENU_BUTTON);

        Condition incognitoTabsExist =
                TabModelConditions.anyIncognitoTabsExist(mTabModelSelectorCondition);
        elements.declareViewIf(REGULAR_TOGGLE_TAB_BUTTON, incognitoTabsExist);
        elements.declareViewIf(INCOGNITO_TOGGLE_TAB_BUTTON, incognitoTabsExist);

        elements.declareLogicalElement(
                uiThreadLogicalElement(
                        "LayoutManager is showing TAB_SWITCHER (Hub)",
                        this::isHubLayoutShowing,
                        mActivityElement));
        elements.declareEnterCondition(new HubLayoutNotInTransition());
    }

    /** Returns the {@link Condition} that acts as {@link Supplier<TabModelSelector>}. */
    public Supplier<TabModelSelector> getTabModelSelectorSupplier() {
        return mTabModelSelectorCondition;
    }

    /** Returns the {@link ChromeTabbedActivity} for this station. */
    public ChromeTabbedActivity getActivity() {
        assertSuppliersCanBeUsed();
        return mActivityElement.get();
    }

    /**
     * Returns to the previous tab via the back button.
     *
     * @return the {@link PageStation} that Hub returned to.
     */
    public <T extends PageStation> T leaveHubToPreviousTabViaBack(T destination) {
        return travelToSync(destination, Transition.retryOption(), () -> Espresso.pressBack());
    }

    /**
     * Selects the tab switcher pane on the Hub.
     *
     * @return the corresponding subclass of {@link HubBaseStation}.
     */
    public <T extends HubBaseStation> T selectPane(
            @PaneId int paneId, Class<T> expectedDestination) {
        recheckActiveConditions();

        if (getPaneId() == paneId) {
            return expectedDestination.cast(this);
        }

        T destinationStation = expectedDestination.cast(HubStationUtils.createHubStation(paneId));

        try {
            HUB_PANE_SWITCHER.onView().check(matches(isDisplayed()));
        } catch (NoMatchingViewException e) {
            throw TravelException.newTravelException(
                    "Hub pane switcher is not visible to switch to " + paneId);
        }

        @StringRes
        int contentDescriptionId = HubStationUtils.getContentDescriptionForIdPaneSelection(paneId);
        return travelToSync(
                destinationStation,
                () -> {
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

    private ConditionStatus isHubLayoutShowing(ChromeTabbedActivity activity) {
        return whether(activity.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
    }

    private void clickPaneSwitcherForPaneWithContentDescription(
            @StringRes int contentDescriptionRes) {
        // TODO(crbug.com/40287437): Content description seems reasonable for now, this might get
        // harder
        // once we use a recycler view with text based buttons.
        String contentDescription = mActivityElement.get().getString(contentDescriptionRes);
        onView(
                        allOf(
                                isDescendantOfA(HUB_PANE_SWITCHER.getViewMatcher()),
                                withContentDescription(contentDescription)))
                .perform(click());
    }

    private class HubLayoutNotInTransition extends UiThreadCondition {
        private HubLayoutNotInTransition() {
            dependOnSupplier(mActivityElement, "ChromeTabbedActivity");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            LayoutManager layoutManager = mActivityElement.get().getLayoutManager();
            boolean startingToShow = layoutManager.isLayoutStartingToShow(LayoutType.TAB_SWITCHER);
            boolean startingToHide = layoutManager.isLayoutStartingToHide(LayoutType.TAB_SWITCHER);
            return whether(
                    !startingToShow && !startingToHide,
                    "startingToShow=%b, startingToHide=%b",
                    startingToShow,
                    startingToHide);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is not in transition to or from TAB_SWITCHER (Hub)";
        }
    }
}
