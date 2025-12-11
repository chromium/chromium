// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertNotEquals;

import android.view.View;

import androidx.annotation.StringRes;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;

/**
 * Facility for a tab switcher card that appears upon long press or right click.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class TabSwitcherTabCardContextMenuFacility<HostStationT extends TabSwitcherStation>
        extends ScrollableFacility<HostStationT> {
    // TODO(crbug.com/467931387): R.id.tab_group_action_menu_list implies tab group-related
    //  operations. Rename to something more appropriate.
    public final ViewElement<TouchTrackingListView> listElement =
            declareView(TouchTrackingListView.class, withId(R.id.tab_group_action_menu_list));

    public Item share;
    public Item addTabToGroup;
    public Item addTabToNewGroup;
    public Item moveTabToGroup;
    public Item addTabToBookmarks;
    public Item editBookmark;
    public Item selectTab;
    public Item pinTab;
    public Item unpinTab;
    public Item closeTab;

    @Override
    protected void declareItems(ItemsBuilder items) {
        share = declarePossibleItemWithText(items, "Share");
        addTabToGroup = declarePossibleItemWithText(items, "Add tab to group");
        addTabToNewGroup = declarePossibleItemWithText(items, "Add tab to new group");
        moveTabToGroup = declarePossibleItemWithText(items, "Move tab to group");
        addTabToBookmarks = declarePossibleItemWithText(items, "Add to bookmarks");
        editBookmark = declarePossibleItemWithText(items, "Edit bookmark");
        selectTab = declarePossibleItemWithText(items, "Select tab");
        pinTab = declarePossibleItemWithText(items, "Pin tab");
        unpinTab = declarePossibleItemWithText(items, "Unpin tab");
        closeTab = declarePossibleItemWithText(items, "Close tab");
    }

    private Item declarePossibleItemWithText(ItemsBuilder builder, String text) {
        return builder.declarePossibleItem(getItemViewSpec(text), withMenuItemTitle(text));
    }

    private ViewSpec<View> getItemViewSpec(String text) {
        return listElement.descendant(withText(text));
    }

    private Matcher<MVCListAdapter.ListItem> withMenuItemTitle(String text) {
        return new TypeSafeMatcher<>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("with menu item title ");
                description.appendText(text);
            }

            @Override
            protected boolean matchesSafely(MVCListAdapter.ListItem listItem) {
                if (listItem.model.containsKey(ListMenuItemProperties.TITLE_ID)) {
                    @StringRes int titleId = listItem.model.get(ListMenuItemProperties.TITLE_ID);
                    assertNotEquals(0, titleId);

                    String title = mHostStation.getActivity().getString(titleId);
                    return text.equals(title);
                }
                return false;
            }
        };
    }
}
