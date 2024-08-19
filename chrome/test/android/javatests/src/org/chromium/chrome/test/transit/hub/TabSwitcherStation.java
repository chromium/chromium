// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.hub.HubToolbarView;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;

/** The base station for Hub tab switcher stations. */
public abstract class TabSwitcherStation extends HubBaseStation {
    public static final ViewSpec TAB_LIST_RECYCLER_VIEW =
            viewSpec(
                    allOf(
                            isDescendantOfA(HubBaseStation.HUB_PANE_HOST.getViewMatcher()),
                            withId(R.id.tab_list_recycler_view)));

    public static final ViewSpec TOOLBAR_NEW_TAB_BUTTON =
            viewSpec(
                    allOf(
                            withId(R.id.toolbar_action_button),
                            isDescendantOfA(instanceOf(HubToolbarView.class))));

    public static final ViewSpec FLOATING_NEW_TAB_BUTTON =
            viewSpec(
                    allOf(
                            withId(R.id.host_action_button),
                            isDescendantOfA(HubBaseStation.HUB_PANE_HOST.getViewMatcher())));

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

    public TabSwitcherStation(
            boolean isIncognito, boolean regularTabsExist, boolean incognitoTabsExist) {
        super(regularTabsExist, incognitoTabsExist);
        mIsIncognito = isIncognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareView(getNewTabButtonViewSpec());
        elements.declareView(TAB_LIST_RECYCLER_VIEW);
    }

    public boolean isIncognito() {
        return mIsIncognito;
    }

    /**
     * Opens the app menu.
     *
     * @return the {@link TabSwitcherAppMenuFacility} for the Hub.
     */
    public TabSwitcherAppMenuFacility openAppMenu() {
        recheckActiveConditions();

        return enterFacilitySync(
                new TabSwitcherAppMenuFacility(mIsIncognito), HUB_MENU_BUTTON::click);
    }

    /**
     * @param index The tab index to select.
     * @param destinationBuilder Builder for the specific type of PageStation expected to appear.
     * @return Builder of the {@link PageStation} for the tab that was selected.
     */
    public <T extends PageStation> T selectTabAtIndex(
            int index, PageStation.Builder<T> destinationBuilder) {
        recheckActiveConditions();

        T destination =
                destinationBuilder
                        .withIncognito(mIsIncognito)
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .build();

        return travelToSync(
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
    public <T extends TabSwitcherStation> T closeTabAtIndex(
            int index, Class<T> expectedDestination) {
        TabModelSelector tabModelSelector = getActivity().getTabModelSelector();
        boolean incognitoModelSelected = tabModelSelector.isOffTheRecordModelSelected();
        int expectedIncognitoTabs = tabModelSelector.getModel(/* incognito= */ true).getCount();
        int expectedRegularTabs = tabModelSelector.getModel(/* incognito= */ false).getCount();

        // By default stay in the same tab switcher state, unless closing the last incognito tab.
        boolean landInIncognitoSwitcher = false;
        if (getPaneId() == PaneId.INCOGNITO_TAB_SWITCHER) {
            assertTrue(incognitoModelSelected);
            expectedIncognitoTabs--;
            if (tabModelSelector.getCurrentModel().getCount() <= 1) {
                landInIncognitoSwitcher = false;
            } else {
                landInIncognitoSwitcher = true;
            }
        } else {
            expectedRegularTabs--;
        }

        T tabSwitcher =
                expectedDestination.cast(
                        HubStationUtils.createHubStation(
                                landInIncognitoSwitcher
                                        ? PaneId.INCOGNITO_TAB_SWITCHER
                                        : PaneId.TAB_SWITCHER,
                                expectedRegularTabs > 0,
                                expectedIncognitoTabs > 0));
        Condition tabCountDecremented =
                new TabCountChangedCondition(
                        tabModelSelector.getModel(incognitoModelSelected),
                        /* expectedChange= */ -1);
        return travelToSync(
                tabSwitcher,
                Transition.conditionOption(tabCountDecremented),
                () -> {
                    ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                            TAB_LIST_RECYCLER_VIEW.getViewMatcher(),
                            index,
                            TAB_CLOSE_BUTTON,
                            click());
                });
    }

    protected ViewSpec getNewTabButtonViewSpec() {
        if (HubFieldTrial.usesFloatActionButton()) {
            return FLOATING_NEW_TAB_BUTTON;
        } else {
            return TOOLBAR_NEW_TAB_BUTTON;
        }
    }

    /**
     * Returns to the previous tab via the back button.
     *
     * @param destinationBuilder Builder for the specific type of PageStation expected to appear.
     * @return the {@link PageStation} that Hub returned to.
     */
    public <T extends PageStation> T leaveHubToPreviousTabViaBack(
            PageStation.Builder<T> destinationBuilder) {
        T destination =
                destinationBuilder
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .withIncognito(mIsIncognito)
                        .build();
        return leaveHubToPreviousTabViaBack(destination);
    }
}
