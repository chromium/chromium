// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.Transition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AppMenuFacility;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;

import java.util.List;

/**
 * App menu shown when in the "Select Tabs" state in the Hub Tab Switcher.
 *
 * <p>Differs significantly from the app menu normally shown; the options are operations to change
 * the tab selection or to do something with the selected tabs.
 */
public class TabListEditorAppMenu extends AppMenuFacility<TabSwitcherStation> {

    private final TabSwitcherListEditorFacility mListEditor;
    private Item<Void> mCloseMenuItem;
    private Item<TabSwitcherGroupCardFacility> mGroupMenuItem;
    private Item<NewTabGroupDialogFacility> mGroupWithParityMenuItem;

    public TabListEditorAppMenu(TabSwitcherListEditorFacility listEditor) {
        mListEditor = listEditor;
    }

    @Override
    protected void declareItems(ScrollableFacility<TabSwitcherStation>.ItemsBuilder items) {
        String tabOrTabs = mListEditor.getNumTabsSelected() > 1 ? "tabs" : "tab";

        // "Select all" usually, or "Deselect all" if all tabs are selected.
        items.declarePossibleStubItem();

        mCloseMenuItem =
                items.declareItem(
                        itemViewMatcher("Close " + tabOrTabs),
                        itemDataMatcher(R.id.tab_list_editor_close_menu_item),
                        this::doCloseTabs);

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            mGroupWithParityMenuItem =
                    items.declareItemToFacility(
                            itemViewMatcher("Group " + tabOrTabs),
                            itemDataMatcher(R.id.tab_list_editor_group_menu_item),
                            this::doGroupTabsWithParityEnabled);
        } else {
            mGroupMenuItem =
                    items.declareItemToFacility(
                            itemViewMatcher("Group " + tabOrTabs),
                            itemDataMatcher(R.id.tab_list_editor_group_menu_item),
                            this::doGroupTabsWithParityDisabled);
        }

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
    public TabSwitcherGroupCardFacility groupTabs() {
        assert !ChromeFeatureList.sTabGroupParityAndroid.isEnabled();
        return mGroupMenuItem.scrollToAndSelect();
    }

    /** Factory for the result of {@link #groupTabs()}. */
    private TabSwitcherGroupCardFacility doGroupTabsWithParityDisabled() {
        return new TabSwitcherGroupCardFacility(mListEditor.getTabIdsSelected());
    }

    /**
     * Select "Group tabs" to create a new group with the selected tabs when TAB_GROUP_PARITY is
     * enabled.
     *
     * @return the "New tab group" dialog as a Facility.
     */
    public NewTabGroupDialogFacility groupTabsWithParityEnabled() {
        assert ChromeFeatureList.sTabGroupParityAndroid.isEnabled();
        return mGroupWithParityMenuItem.scrollToAndSelect();
    }

    /** Factory for the result of {@link #groupTabsWithParityEnabled()}. */
    private NewTabGroupDialogFacility doGroupTabsWithParityEnabled() {
        return new NewTabGroupDialogFacility(mListEditor.getTabIdsSelected());
    }

    /**
     * Select "Close tabs" to close all selected tabs.
     *
     * @return the next state of the TabSwitcher as a Station and the newly created tab group card
     *     as a Facility.
     */
    public Void closeTabs() {
        return mCloseMenuItem.scrollToAndSelect();
    }

    public Void doCloseTabs(ItemOnScreenFacility<Void> itemOnScreen) {
        TabModel tabModel =
                mHostStation
                        .getTabModelSelectorSupplier()
                        .get()
                        .getModel(mHostStation.isIncognito());
        Condition tabCountDecreased =
                new TabCountChangedCondition(tabModel, -mListEditor.getNumTabsSelected());
        mHostStation.exitFacilitiesSync(
                List.of(this, mListEditor, itemOnScreen),
                Transition.conditionOption(tabCountDecreased),
                itemOnScreen.clickTrigger());

        return null;
    }
}
