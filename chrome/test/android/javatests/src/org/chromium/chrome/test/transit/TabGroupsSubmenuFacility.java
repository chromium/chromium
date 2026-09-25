// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.anyOf;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.widget.ListView;

import androidx.annotation.IdRes;

import org.chromium.base.test.transit.RootSpec;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Facility representing the Tab Groups Submenu popup in the App Menu. */
public class TabGroupsSubmenuFacility<
                HostStationT extends ChromeActivityTabModelBoundStation<ChromeTabbedActivity>>
        extends ScrollableFacility<HostStationT> {
    private final AppMenuFacility<?> mParentAppMenu;
    private final List<String> mExpectedGroups;
    private final List<String> mExcludedGroups;
    private final Map<String, Item> mGroupItems = new HashMap<>();
    private Item mCreateNewTabGroupItem;
    private Item mAddToGroupItem;

    public TabGroupsSubmenuFacility(
            AppMenuFacility<?> parentAppMenu,
            List<String> expectedGroups,
            List<String> excludedGroups) {
        mParentAppMenu = parentAppMenu;
        mExpectedGroups = expectedGroups;
        mExcludedGroups = excludedGroups;
        declareContainerView(
                ListView.class,
                allOf(withId(R.id.app_menu_list), isDisplayed(), hasDescendant(withId(R.id.create_new_tab_group_menu_id))),
                ViewElement.newOptions().rootSpec(RootSpec.focusedRoot()).build());
    }

    protected Item declareMenuItem(ItemsBuilder items, @IdRes int id) {
        return items.declareItem(withId(id), AppMenuFacility.withMenuItemId(id));
    }

    protected Item declarePossibleMenuItem(ItemsBuilder items, @IdRes int id) {
        return items.declarePossibleItem(viewSpec(withId(id)), AppMenuFacility.withMenuItemId(id));
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        mCreateNewTabGroupItem = declarePossibleMenuItem(items, R.id.create_new_tab_group_menu_id);
        mAddToGroupItem = declarePossibleMenuItem(items, R.id.add_to_group_menu_id);
        for (String title : mExpectedGroups) {
            Item item =
                    items.declareItem(
                            allOf(
                                    anyOf(
                                            withId(R.id.tab_group_menu_item_id),
                                            withId(R.id.add_to_existing_group_menu_item_id)),
                                    hasDescendant(withText(title))),
                            null);
            mGroupItems.put(title, item);
        }
        for (String title : mExcludedGroups) {
            items.declareAbsentItem(
                    viewSpec(
                            allOf(
                                    anyOf(withId(R.id.tab_group_menu_item_id), withId(R.id.add_to_existing_group_menu_item_id)),
                                    hasDescendant(withText(title)))),
                    null);
        }
    }

    /** Selects "New tab group" from the submenu to create a group for the current tab via UI. */
    public NewTabGroupDialogFacility<HostStationT> clickNewTabGroup() {
        return Journeys.beginNewTabGroupUiFlow(
                mCreateNewTabGroupItem.scrollToAndSelectTo().exitFacilityAnd(mParentAppMenu));
    }

    /**
     * Selects "Add to group" from the submenu, opening the {@link AddToGroupSubmenuFacility}.
     *
     * @param expectedGroups List of group titles expected to be present in the submenu.
     * @param excludedGroups List of group titles expected to be absent from the submenu.
     * @return the {@link AddToGroupSubmenuFacility}.
     */
    public AddToGroupSubmenuFacility<HostStationT> openAddToGroupSubmenu(
            List<String> expectedGroups, List<String> excludedGroups) {
        assertNotNull("Add to group menu item not found", mAddToGroupItem);
        return mAddToGroupItem
                .scrollToAndSelectWithoutClosingTo()
                .enterFacility(
                        new AddToGroupSubmenuFacility<>(
                                mParentAppMenu, this, expectedGroups, excludedGroups));
    }

    /** Selects a tab group item from the submenu, exiting both the submenu and parent menu. */
    public void selectGroup(String groupTitle) {
        Item item = mGroupItems.get(groupTitle);
        assertNotNull("Group item not declared in expected groups: " + groupTitle, item);
        item.scrollToAndSelectTo().exitFacility(mParentAppMenu);
    }
}
