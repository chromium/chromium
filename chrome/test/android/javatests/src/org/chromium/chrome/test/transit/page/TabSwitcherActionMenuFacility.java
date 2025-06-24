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
import org.chromium.base.test.transit.TripBuilder;
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
                    && mHostStation.getTabModelSelector().getModel(false).getCount() > 0) {
                switchOutOfIncognitoMenuItemElement =
                        declareView(
                                appMenuListElement.descendant(
                                        withText(R.string.menu_switch_out_of_incognito)));
            } else if (!mHostStation.isIncognito()
                    && mHostStation.getTabModelSelector().getModel(true).getCount() > 0) {
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
        TabModelSelector tabModelSelector = mHostStation.getTabModelSelector();
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

        return selectCloseTabTo()
                .arriveAt(
                        new RegularTabSwitcherStation(
                                /* regularTabsExist= */ false, incognitoTabCount > 0));
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on another tab
     * in the same TabModel.
     *
     * <p>This happens when there are other tabs in the same TabModel.
     */
    public <T extends PageStation> T selectCloseTabAndDisplayAnotherTab(
            PageStation.Builder<T> pageStationBuilder) {
        return selectCloseTabTo()
                .arriveAt(pageStationBuilder.initFrom(mHostStation).withIsSelectingTabs(1).build());
    }

    /**
     * Select the "Close tab" menu option to close the current Tab, expecting to land on a regular
     * tab.
     *
     * <p>This happens when the last incognito tab is closed but there are other regular tabs.
     */
    public <T extends PageStation> T selectCloseTabAndDisplayRegularTab(
            PageStation.Builder<T> pageStationBuilder) {
        return selectCloseTabTo()
                .arriveAt(
                        pageStationBuilder.withIncognito(false).initSelectingExistingTab().build());
    }

    public TripBuilder selectCloseTabTo() {
        return closeTabMenuItemElement
                .clickTo()
                .waitForAnd(createTabCountChangedCondition(mHostStation.isIncognito(), -1));
    }

    /** Select the "New tab" menu option to open a new Tab. */
    public RegularNewTabPageStation selectNewTab() {
        RegularNewTabPageStation destination =
                RegularNewTabPageStation.newBuilder().initOpeningNewTab().build();
        return newTabMenuItemElement
                .clickTo()
                .waitForAnd(createTabCountChangedCondition(/* incognito= */ false, +1))
                .arriveAt(destination);
    }

    /** Select the "New Incognito tab" menu option to open a new incognito Tab. */
    public IncognitoNewTabPageStation selectNewIncognitoTab() {
        return newIncognitoTabMenuItemElement
                .clickTo()
                .waitForAnd(createTabCountChangedCondition(/* incognito= */ true, +1))
                .arriveAt(IncognitoNewTabPageStation.newBuilder().initOpeningNewTab().build());
    }

    /** Switches out of incognito tab model to regular tab model */
    public <T extends PageStation> T selectSwitchOutOfIncognito(
            PageStation.Builder<T> destinationBuilder) {
        assertTrue(mHostStation.isIncognito());
        return switchOutOfIncognitoMenuItemElement
                .clickTo()
                .waitForAnd(createTabModelChangedCondition())
                .arriveAt(destinationBuilder.initSelectingExistingTab().build());
    }

    /** Switches to incognito tab model from regular tab model */
    public <T extends PageStation> T selectSwitchToIncognito(
            PageStation.Builder<T> destinationBuilder) {
        assertFalse(mHostStation.isIncognito());
        return switchToIncognitoMenuItemElement
                .clickTo()
                .waitForAnd(createTabModelChangedCondition())
                .arriveAt(destinationBuilder.initSelectingExistingTab().build());
    }

    private Condition createTabCountChangedCondition(boolean incognito, int change) {
        return new TabCountChangedCondition(
                mHostStation.getTabModelSelector().getModel(incognito), change);
    }

    private Condition createTabModelChangedCondition() {
        return new TabModelChangedCondition(mHostStation.getTabModelSelector());
    }
}
