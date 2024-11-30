// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.test.espresso.Espresso;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;

import java.util.ArrayList;
import java.util.List;

/** The 3-dot menu "Select Tabs" UI for the {@link TabSwitcherStation} panes. */
// TODO(crbug/324919909): Migrate TabListEditorTestingRobot to here.
public class TabSwitcherListEditorFacility extends Facility<TabSwitcherStation> {
    public static final ViewSpec TAB_LIST_EDITOR_LAYOUT = viewSpec(withId(R.id.selectable_list));
    public static final ViewSpec TAB_LIST_EDITOR_RECYCLER_VIEW =
            viewSpec(
                    allOf(
                            isDescendantOfA(withId(R.id.selectable_list)),
                            withId(R.id.tab_list_recycler_view)));

    private final List<Integer> mTabIdsSelected;
    private final List<List<Integer>> mTabGroupsSelected;

    public TabSwitcherListEditorFacility(
            List<Integer> tabIdsSelected, List<List<Integer>> tabGroupsSelected) {
        mTabIdsSelected = tabIdsSelected;
        mTabGroupsSelected = tabGroupsSelected;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(TAB_LIST_EDITOR_LAYOUT);
        elements.declareView(TAB_LIST_EDITOR_RECYCLER_VIEW);
        Matcher<View> viewMatcher =
                allOf(
                        withText(getSelectionModeNumberText()),
                        withId(R.id.down),
                        withParent(withId(R.id.selection_mode_number)));
        elements.declareView(viewSpec(viewMatcher));
    }

    private String getSelectionModeNumberText() {
        if (getNumTabsSelected() == 0) {
            return "Select tabs";
        } else {
            return TabGroupUtil.getNumberOfTabsString(getNumTabsSelected());
        }
    }

    public List<Integer> getAllTabIdsSelected() {
        List<Integer> allTabIds = new ArrayList<>(mTabIdsSelected);
        for (List<Integer> tabGroupIds : mTabGroupsSelected) {
            allTabIds.addAll(tabGroupIds);
        }
        return allTabIds;
    }

    public int getNumTabsSelected() {
        int totalTabs = mTabIdsSelected.size();
        for (List<Integer> tabGroupIds : mTabGroupsSelected) {
            totalTabs += tabGroupIds.size();
        }
        return totalTabs;
    }

    /** Presses back to exit the facility. */
    public void pressBackToExit() {
        mHostStation.exitFacilitySync(
                this,
                () -> {
                    Espresso.pressBack();
                });
    }

    /** Add a tab in the grid to the selection. */
    public TabSwitcherListEditorFacility addTabToSelection(int index, int tabId) {
        List<Integer> newTabIdsSelected = new ArrayList<>(mTabIdsSelected);
        newTabIdsSelected.add(tabId);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherListEditorFacility(newTabIdsSelected, mTabGroupsSelected),
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItem(
                                TAB_LIST_EDITOR_RECYCLER_VIEW.getViewMatcher(), index, click()));
    }

    /** Add a tab group in the grid to the selection. */
    public TabSwitcherListEditorFacility addTabGroupToSelection(int index, List<Integer> tabIds) {
        List<List<Integer>> newTabGroupsSelected = new ArrayList<>(mTabGroupsSelected);
        newTabGroupsSelected.add(tabIds);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherListEditorFacility(mTabIdsSelected, newTabGroupsSelected),
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItem(
                                TAB_LIST_EDITOR_RECYCLER_VIEW.getViewMatcher(), index, click()));
    }

    /** Open the app menu, which looks different while selecting tabs. */
    public TabListEditorAppMenu openAppMenuWithEditor() {
        return mHostStation.enterFacilitySync(
                new TabListEditorAppMenu(this), HubBaseStation.HUB_MENU_BUTTON::click);
    }

    /**
     * Returns whether any group is selected. Relevant because a group does not get created in this
     * case, but rather all tabs get moved to one of the selected groups.
     */
    public boolean isAnyGroupSelected() {
        return !mTabGroupsSelected.isEmpty();
    }
}
