// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;

import java.util.ArrayList;
import java.util.List;

/**
 * The 3-dot menu "Select Tabs" UI for the {@link TabSwitcherStation} panes.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
// TODO(crbug/324919909): Migrate TabListEditorTestingRobot to here.
public class TabSwitcherListEditorFacility<HostStationT extends TabSwitcherStation>
        extends Facility<HostStationT> {
    private final List<Integer> mTabIdsSelected;
    private final List<List<Integer>> mTabGroupsSelected;
    public final ViewElement<View> editorLayoutElement;
    public final ViewElement<RecyclerView> tabListRecyclerViewElement;
    public final ViewElement<View> selectionTitleElement;

    public TabSwitcherListEditorFacility(
            List<Integer> tabIdsSelected, List<List<Integer>> tabGroupsSelected) {
        mTabIdsSelected = tabIdsSelected;
        mTabGroupsSelected = tabGroupsSelected;

        editorLayoutElement = declareView(withId(R.id.selectable_list));
        tabListRecyclerViewElement =
                declareView(
                        editorLayoutElement.descendant(
                                RecyclerView.class, withId(R.id.tab_list_recycler_view)));
        selectionTitleElement =
                declareView(
                        viewSpec(
                                withText(getSelectionModeNumberText()),
                                withId(R.id.down),
                                withParent(withId(R.id.selection_mode_number))));
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
        mHostStation.exitFacilitySync(this, Espresso::pressBack);
    }

    /** Add a tab in the grid to the selection. */
    public TabSwitcherListEditorFacility<HostStationT> addTabToSelection(int index, int tabId) {
        List<Integer> newTabIdsSelected = new ArrayList<>(mTabIdsSelected);
        newTabIdsSelected.add(tabId);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherListEditorFacility<>(newTabIdsSelected, mTabGroupsSelected),
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItem(
                                tabListRecyclerViewElement.getViewSpec().getViewMatcher(),
                                index,
                                click()));
    }

    /** Add a tab group in the grid to the selection. */
    public TabSwitcherListEditorFacility<HostStationT> addTabGroupToSelection(
            int index, List<Integer> tabIds) {
        List<List<Integer>> newTabGroupsSelected = new ArrayList<>(mTabGroupsSelected);
        newTabGroupsSelected.add(tabIds);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherListEditorFacility<>(mTabIdsSelected, newTabGroupsSelected),
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItem(
                                tabListRecyclerViewElement.getViewSpec().getViewMatcher(),
                                index,
                                click()));
    }

    /** Open the app menu, which looks different while selecting tabs. */
    public TabListEditorAppMenu<HostStationT> openAppMenuWithEditor() {
        return mHostStation.enterFacilitySync(
                new TabListEditorAppMenu<>(this), mHostStation.menuButtonElement.getClickTrigger());
    }

    /**
     * Returns whether any group is selected. Relevant because a group does not get created in this
     * case, but rather all tabs get moved to one of the selected groups.
     */
    public boolean isAnyGroupSelected() {
        return !mTabGroupsSelected.isEmpty();
    }
}
