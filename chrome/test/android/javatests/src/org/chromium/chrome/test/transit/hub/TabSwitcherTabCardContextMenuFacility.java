// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.SimpleConditions.uiThreadCondition;
import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import androidx.annotation.StringRes;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.tabmodel.TabsPinnedStatusCondition;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;

import java.util.ArrayList;
import java.util.List;

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
            declareContainerView(
                    TouchTrackingListView.class,
                    withId(R.id.tab_group_action_menu_list),
                    ViewElement.initialSettleTimeOption(1000));
    private final @TabId int mTabId;

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

    /**
     * @param tabId the id of the tab that this context menu is for.
     */
    public TabSwitcherTabCardContextMenuFacility(@TabId int tabId) {
        mTabId = tabId;
    }

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

    /**
     * Click 'Add to group'. This should only be possible when there is at least one tab group.
     *
     * @param isNewTabGroupRowVisible Whether the 'new tab group' row should be visible.
     */
    public TabGroupListBottomSheetFacility<HostStationT> clickAddTabToGroup(
            boolean isNewTabGroupRowVisible) {
        checkItemsAbsent(addTabToNewGroup, moveTabToGroup);

        List<Token> allTabGroupIds =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new ArrayList<>(
                                        mHostStation.getTabGroupModelFilter().getAllTabGroupIds()));
        return addTabToGroup
                .scrollToAndSelectTo()
                .enterFacility(
                        new TabGroupListBottomSheetFacility<>(
                                allTabGroupIds, isNewTabGroupRowVisible));
    }

    /** Click add to new group. This should only be possible when there are no tab groups. */
    public NewTabGroupDialogFacility<HostStationT> clickAddTabToNewGroup() {
        checkItemsAbsent(addTabToGroup, moveTabToGroup);

        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        NewTabGroupDialogFacility<HostStationT> newTabGroupDialogFacility =
                new NewTabGroupDialogFacility<>(List.of(mTabId), softKeyboard);
        newTabGroupDialogFacility =
                addTabToNewGroup
                        .scrollToAndSelectTo()
                        .enterFacilityAnd(softKeyboard)
                        .enterFacility(newTabGroupDialogFacility);
        softKeyboard.close(newTabGroupDialogFacility.dialogElement);
        return newTabGroupDialogFacility;
    }

    /** Click Pin Tab. This should only be possible if the tab is unpinned. */
    public void pinTab() {
        checkItemsAbsent(unpinTab);
        changePinnedState(/* newPinnedState= */ true, pinTab);
    }

    /** Click Unpin Tab. This should only be possible if the tab is pinned. */
    public void unpinTab() {
        checkItemsAbsent(pinTab);
        changePinnedState(/* newPinnedState= */ false, unpinTab);
    }

    /** Click Select tab and open List Editor. */
    public TabSwitcherListEditorFacility<HostStationT> selectTab() {
        return selectTab
                .scrollToAndSelectTo()
                .enterFacility(new TabSwitcherListEditorFacility<>(List.of(mTabId), List.of()));
    }

    /** Click Close tab. */
    public void closeTab() {
        // TODO(crbug.com/470054396): Check that the tab card was removed from the tab switcher.
        closeTab.scrollToAndSelectTo()
                .waitFor(
                        uiThreadCondition(
                                "Tab was closed",
                                () ->
                                        whether(
                                                mHostStation.getTabModel().getTabById(mTabId)
                                                        == null)));
    }

    private void changePinnedState(boolean newPinnedState, Item pinListItem) {
        assertTrue(ChromeFeatureList.sAndroidPinnedTabs.isEnabled());

        noopTo().waitFor(
                        new TabsPinnedStatusCondition(
                                mHostStation.getTabModel(), List.of(mTabId), !newPinnedState));

        pinListItem
                .scrollToAndSelectTo()
                .waitFor(
                        new TabsPinnedStatusCondition(
                                mHostStation.getTabModel(), List.of(mTabId), newPinnedState));
    }

    private Item declarePossibleItemWithText(ItemsBuilder builder, String text) {
        return builder.declarePossibleItem(viewSpec(withText(text)), withMenuItemTitle(text));
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
