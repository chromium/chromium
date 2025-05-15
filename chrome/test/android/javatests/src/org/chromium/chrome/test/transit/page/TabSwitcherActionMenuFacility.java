// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.View;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.transit.tabmodel.TabModelChangedCondition;

/** The action menu opened when long pressing the tab switcher button in a {@link PageStation}. */
public class TabSwitcherActionMenuFacility extends Facility<PageStation> {
    public ViewElement<View> appMenuListElement;
    public ViewElement<View> closeTabMenuItemElement;
    public ViewElement<View> newTabMenuItemElement;
    public ViewElement<View> newIncognitoTabMenuItemElement;
    public ViewElement<View> switchOutOfIncognitoMenuItemElement;
    public ViewElement<View> switchToIncognitoMenuItemElement;

    @Override
    public void declareExtraElements() {
        appMenuListElement = declareView(withId(R.id.app_menu_list));
        closeTabMenuItemElement =
                declareView(appMenuListElement.descendant(withText(R.string.close_tab)));
        newTabMenuItemElement =
                declareView(appMenuListElement.descendant(withText(R.string.menu_new_tab)));
        newIncognitoTabMenuItemElement =
                declareView(
                        appMenuListElement.descendant(withText(R.string.menu_new_incognito_tab)));

        if (ChromeFeatureList.sTabStripIncognitoMigration.isEnabled()) {
            if (mHostStation.isIncognito()
                    && mHostStation.getActivity().getTabModelSelector().getModel(false).getCount()
                            > 0) {
                switchOutOfIncognitoMenuItemElement =
                        declareView(
                                appMenuListElement.descendant(
                                        withText(R.string.menu_switch_out_of_incognito)));
            } else if (!mHostStation.isIncognito()
                    && mHostStation.getActivity().getTabModelSelector().getModel(true).getCount()
                            > 0) {
                switchToIncognitoMenuItemElement =
                        declareView(
                                appMenuListElement.descendant(
                                        withText(R.string.menu_switch_to_incognito)));
            }
        }
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on the regular
     * Tab Switcher.
     *
     * <p>This happens when the last regular tab is closed or when the last incognito is closed.
     */
    public RegularTabSwitcherStation selectCloseTabAndDisplayTabSwitcher() {
        TabModelSelector tabModelSelector = mHostStation.getActivity().getTabModelSelector();
        int incognitoTabCount = tabModelSelector.getModel(/* incognito= */ true).getCount();
        int regularTabCount = tabModelSelector.getModel(/* incognito= */ false).getCount();
        if (mHostStation.isIncognito()) {
            incognitoTabCount--;
            assertEquals(
                    "Another incognito tab should be selected instead of entering the tab switcher",
                    0,
                    incognitoTabCount);
        } else {
            regularTabCount--;
        }
        assertEquals(
                "Another regular tab should be selected instead of entering the tab switcher",
                0,
                regularTabCount);

        RegularTabSwitcherStation destination =
                new RegularTabSwitcherStation(/* regularTabsExist= */ false, incognitoTabCount > 0);
        return selectCloseTab(destination);
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on another tab
     * in the same TabModel.
     *
     * <p>This happens when there are other tabs in the same TabModel.
     */
    public <T extends PageStation> T selectCloseTabAndDisplayAnotherTab(
            PageStation.Builder<T> pageStationBuilder) {
        T destination = pageStationBuilder.initFrom(mHostStation).withIsSelectingTabs(1).build();

        return selectCloseTab(destination);
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on a regular
     * tab.
     *
     * <p>This happens when the last incognito tab is closed but there are other regular tabs.
     */
    public <T extends PageStation> T selectCloseTabAndDisplayRegularTab(
            PageStation.Builder<T> pageStationBuilder) {
        T destination =
                pageStationBuilder
                        .withIncognito(false)
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .build();

        return selectCloseTab(destination);
    }

    private <T extends Station<?>> T selectCloseTab(T destination) {
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(
                        createTabCountChangedCondition(mHostStation.isIncognito(), -1)),
                closeTabMenuItemElement.getClickTrigger());
    }

    /** Select the "New tab" menu option to open a new Tab. */
    public RegularNewTabPageStation selectNewTab() {
        RegularNewTabPageStation destination =
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(
                        createTabCountChangedCondition(/* incognito= */ false, +1)),
                newTabMenuItemElement.getClickTrigger());
    }

    /** Select the "New Incognito tab" menu option to open a new incognito Tab. */
    public IncognitoNewTabPageStation selectNewIncognitoTab() {
        IncognitoNewTabPageStation destination =
                IncognitoNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(
                        createTabCountChangedCondition(/* incognito= */ true, +1)),
                newIncognitoTabMenuItemElement.getClickTrigger());
    }

    /** Switches out of incognito tab model to regular tab model */
    public <T extends PageStation> T selectSwitchOutOfIncognito(
            PageStation.Builder<T> destinationBuilder) {
        assertTrue(mHostStation.isIncognito());
        T destination = destinationBuilder.withIsOpeningTabs(0).withIsSelectingTabs(1).build();
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(createTabModelChangedCondition()),
                switchOutOfIncognitoMenuItemElement.getClickTrigger());
    }

    /** Switches to incognito tab model from regular tab model */
    public <T extends PageStation> T selectSwitchToIncognito(
            PageStation.Builder<T> destinationBuilder) {
        assertFalse(mHostStation.isIncognito());
        T destination = destinationBuilder.withIsOpeningTabs(0).withIsSelectingTabs(1).build();
        return mHostStation.travelToSync(
                destination,
                Transition.conditionOption(createTabModelChangedCondition()),
                switchToIncognitoMenuItemElement.getClickTrigger());
    }

    private Condition createTabCountChangedCondition(boolean incognito, int change) {
        return new TabCountChangedCondition(
                mHostStation.getActivity().getTabModelSelector().getModel(incognito), change);
    }

    private Condition createTabModelChangedCondition() {
        return new TabModelChangedCondition(mHostStation.getActivity().getTabModelSelector());
    }
}
