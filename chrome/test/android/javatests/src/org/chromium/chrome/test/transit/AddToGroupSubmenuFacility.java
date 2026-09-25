// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.widget.ListView;

import org.chromium.base.test.transit.RootSpec;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Facility representing the Add to Group Submenu popup in the App Menu. */
public class AddToGroupSubmenuFacility<
                HostStationT extends ChromeActivityTabModelBoundStation<ChromeTabbedActivity>>
        extends ScrollableFacility<HostStationT> {
    private final AppMenuFacility<?> mParentAppMenu;
    private final TabGroupsSubmenuFacility<HostStationT> mTabGroupsSubmenu;
    private final List<String> mExpectedGroups;
    private final List<String> mExcludedGroups;
    private final Map<String, Item> mGroupItems = new HashMap<>();

    public AddToGroupSubmenuFacility(
            AppMenuFacility<?> parentAppMenu,
            TabGroupsSubmenuFacility<HostStationT> tabGroupsSubmenu,
            List<String> expectedGroups,
            List<String> excludedGroups) {
        mParentAppMenu = parentAppMenu;
        mTabGroupsSubmenu = tabGroupsSubmenu;
        mExpectedGroups = expectedGroups;
        mExcludedGroups = excludedGroups;
        declareContainerView(
                ListView.class,
                allOf(
                        withId(R.id.app_menu_list),
                        isDisplayed(),
                        hasDescendant(withId(R.id.add_to_existing_group_menu_item_id))),
                ViewElement.newOptions().rootSpec(RootSpec.focusedRoot()).build());
    }

    @Override
    protected void declareItems(ItemsBuilder items) {
        for (String title : mExpectedGroups) {
            Item item =
                    items.declareItem(
                            allOf(
                                    withId(R.id.add_to_existing_group_menu_item_id),
                                    hasDescendant(withText(title))),
                            null);
            mGroupItems.put(title, item);
        }
        for (String title : mExcludedGroups) {
            items.declareAbsentItem(
                    viewSpec(
                            allOf(
                                    withId(R.id.add_to_existing_group_menu_item_id),
                                    hasDescendant(withText(title)))),
                    null);
        }
    }

    /** Selects a tab group item from the submenu, merging into it and exiting all parent menus. */
    public void selectGroup(String groupTitle) {
        Item item = mGroupItems.get(groupTitle);
        assertNotNull("Group item not declared in expected groups: " + groupTitle, item);
        item.scrollToAndSelectTo().exitFacilities(mTabGroupsSubmenu, mParentAppMenu);
    }
}
