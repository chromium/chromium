// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.browser.hub.HubToolbarView;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;

/** The base station for Hub tab switcher stations. */
public abstract class HubTabSwitcherBaseStation extends HubBaseStation {
    public static final ViewElement TAB_LIST_RECYCLER_VIEW =
            sharedViewElement(
                    allOf(
                            isDescendantOfA(HubBaseStation.HUB_PANE_HOST.getViewMatcher()),
                            withId(R.id.tab_list_recycler_view)));

    public static final ViewElement TOOLBAR_NEW_TAB_BUTTON =
            sharedViewElement(
                    allOf(
                            withId(R.id.toolbar_action_button),
                            isDescendantOfA(instanceOf(HubToolbarView.class))));

    public static final Matcher<View> TAB_CLOSE_BUTTON =
            allOf(
                    withId(R.id.action_button),
                    isDescendantOfA(
                            allOf(
                                    withId(R.id.content_view),
                                    withParent(instanceOf(TabGridView.class)))),
                    isDisplayed());
    public static final Matcher<View> TAB_THUMBNAIL =
            allOf(
                    withId(R.id.tab_thumbnail),
                    isDescendantOfA(
                            allOf(
                                    withId(R.id.content_view),
                                    withParent(instanceOf(TabGridView.class)))),
                    isDisplayed());

    private final boolean mIsIncognito;

    /**
     * @param chromeTabbedActivityTestRule The activity rule under test.
     */
    public HubTabSwitcherBaseStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean isIncognito) {
        super(chromeTabbedActivityTestRule);
        mIsIncognito = isIncognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareView(TOOLBAR_NEW_TAB_BUTTON);
        elements.declareView(TAB_LIST_RECYCLER_VIEW);
    }

    /**
     * Opens the app menu.
     *
     * @return the {@link HubTabSwitcherAppMenuFacility} for the Hub.
     */
    public HubTabSwitcherAppMenuFacility openAppMenu() {
        recheckActiveConditions();

        HubTabSwitcherAppMenuFacility menu = new HubTabSwitcherAppMenuFacility(this, mIsIncognito);

        return Facility.enterSync(menu, () -> HUB_MENU_BUTTON.perform(click()));
    }

    /**
     * @param index The tab index to select.
     * @return the {@link PageStation} for the tab that was selected.
     */
    public PageStation selectTabAtIndex(int index) {
        recheckActiveConditions();

        PageStation destination =
                PageStation.newPageStationBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIncognito(mIsIncognito)
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .build();

        return Trip.travelSync(
                this,
                destination,
                () -> {
                    ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                            TAB_LIST_RECYCLER_VIEW.getViewMatcher(), index, TAB_THUMBNAIL, click());
                });
    }

    /**
     * Close a tab and end in a destination.
     *
     * @param index The index of the tab to close.
     */
    public <T extends HubTabSwitcherBaseStation> T closeTabAtIndex(
            int index, Class<T> expectedDestination) {
        TabModelSelector tabModelSelector =
                mChromeTabbedActivityTestRule.getActivity().getTabModelSelector();

        // By default stay in the same tab switcher state, unless closing the last incognito tab.
        boolean landInIncognitoSwitcher = false;
        if (getPaneId() == PaneId.INCOGNITO_TAB_SWITCHER) {
            assertTrue(tabModelSelector.isIncognitoSelected());
            if (tabModelSelector.getCurrentModel().getCount() <= 1) {
                landInIncognitoSwitcher = false;
            } else {
                landInIncognitoSwitcher = true;
            }
        }

        T tabSwitcher =
                expectedDestination.cast(
                        HubStationUtils.createHubStation(
                                landInIncognitoSwitcher
                                        ? PaneId.INCOGNITO_TAB_SWITCHER
                                        : PaneId.TAB_SWITCHER,
                                mChromeTabbedActivityTestRule));

        return Trip.travelSync(
                this,
                tabSwitcher,
                () -> {
                    ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                            TAB_LIST_RECYCLER_VIEW.getViewMatcher(),
                            index,
                            TAB_CLOSE_BUTTON,
                            click());
                });
    }

    /** Open a new tab using the New Tab action button. */
    public PageStation openNewTab() {
        recheckActiveConditions();

        PageStation page =
                PageStation.newPageStationBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIncognito(mIsIncognito)
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return Trip.travelSync(this, page, () -> TOOLBAR_NEW_TAB_BUTTON.perform(click()));
    }
}
