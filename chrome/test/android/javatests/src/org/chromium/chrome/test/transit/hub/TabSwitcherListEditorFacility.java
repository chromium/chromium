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

    public TabSwitcherListEditorFacility(List<Integer> tabIdsSelected) {
        mTabIdsSelected = tabIdsSelected;
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

    public List<Integer> getTabIdsSelected() {
        return mTabIdsSelected;
    }

    public int getNumTabsSelected() {
        return mTabIdsSelected.size();
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
        List<Integer> newTabIdsList = new ArrayList<>(mTabIdsSelected);
        newTabIdsList.add(tabId);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherListEditorFacility(newTabIdsList),
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItem(
                                TAB_LIST_EDITOR_RECYCLER_VIEW.getViewMatcher(), index, click()));
    }

    /** Open the app menu, which looks different while selecting tabs. */
    public TabListEditorAppMenu openAppMenuWithEditor() {
        return mHostStation.enterFacilitySync(
                new TabListEditorAppMenu(this), HubBaseStation.HUB_MENU_BUTTON::click);
    }
}
