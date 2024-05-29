// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.click;

import android.util.Pair;

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AppMenuFacility;
import org.chromium.chrome.test.transit.HubTabSwitcherBaseStation;
import org.chromium.chrome.test.transit.HubTabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.HubTabSwitcherStation;

/**
 * App menu shown when in the "Select Tabs" state in the Hub Tab Switcher.
 *
 * <p>Differs significantly from the app menu normally shown; the options are operations to change
 * the tab selection or to do something with the selected tabs.
 */
public class HubTabListEditorAppMenu extends AppMenuFacility<HubTabSwitcherBaseStation> {

    private final HubTabSwitcherListEditorFacility mListEditor;
    private Item<Pair<HubTabSwitcherStation, HubTabSwitcherGroupCardFacility>> mGroupMenuItems;

    public HubTabListEditorAppMenu(
            HubTabSwitcherBaseStation station, HubTabSwitcherListEditorFacility listEditor) {
        super(station);
        mListEditor = listEditor;
    }

    @Override
    protected void declareItems(ScrollableFacility<HubTabSwitcherBaseStation>.ItemsBuilder items) {
        String tabOrTabs = mListEditor.getNumTabsSelected() > 1 ? "tabs" : "tab";

        // "Select all" usually, or "Deselect all" if all tabs are selected.
        items.declarePossibleStubItem();

        items.declareStubItem(
                itemViewMatcher("Close " + tabOrTabs),
                itemDataMatcher(R.id.tab_list_editor_close_menu_item));

        mGroupMenuItems =
                items.declareItem(
                        itemViewMatcher("Group " + tabOrTabs),
                        itemDataMatcher(R.id.tab_list_editor_group_menu_item),
                        this::doGroupTabs);

        items.declareStubItem(
                itemViewMatcher("Bookmark " + tabOrTabs),
                itemDataMatcher(R.id.tab_list_editor_bookmark_menu_item));

        items.declareStubItem(
                itemViewMatcher("Share " + tabOrTabs),
                itemDataMatcher(R.id.tab_list_editor_share_menu_item));
    }

    /**
     * Select "Group tabs" to create a new group with the selected tabs.
     *
     * @return the next state of the TabSwitcher as a Station and the newly created tab group card
     *     as a Facility.
     */
    public Pair<HubTabSwitcherStation, HubTabSwitcherGroupCardFacility> groupTabs() {
        return mGroupMenuItems.scrollToAndSelect();
    }

    /** Actual implementation of {@link #groupTabs()}. Called after scrolling if necessary. */
    private Pair<HubTabSwitcherStation, HubTabSwitcherGroupCardFacility> doGroupTabs(
            ItemOnScreenFacility<Pair<HubTabSwitcherStation, HubTabSwitcherGroupCardFacility>>
                    itemOnScreen) {
        HubTabSwitcherStation tabSwitcherAfter = new HubTabSwitcherStation();
        HubTabSwitcherGroupCardFacility groupCard =
                new HubTabSwitcherGroupCardFacility(
                        tabSwitcherAfter, mListEditor.getTabIdsSelected());
        tabSwitcherAfter.addInitialFacility(groupCard);
        mHostStation.travelToSync(
                tabSwitcherAfter, () -> mGroupMenuItems.getViewElement().perform(click()));
        return Pair.create(tabSwitcherAfter, groupCard);
    }
}
