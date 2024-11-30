// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import android.util.Pair;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.Transition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AppMenuFacility;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;

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
    private Item<NewTabGroupDialogFacility> mGroupWithDialogMenuItem;
    private Item<Pair<TabSwitcherGroupCardFacility, UndoGroupSnackbarFacility>>
            mGroupWithoutDialogMenuItem;

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

        if (mListEditor.isAnyGroupSelected()) {
            mGroupWithoutDialogMenuItem =
                    items.declareItem(
                            itemViewMatcher("Group " + tabOrTabs),
                            itemDataMatcher(R.id.tab_list_editor_group_menu_item),
                            this::doGroupTabsWithoutDialog);
        } else {
            mGroupWithDialogMenuItem =
                    items.declareItem(
                            itemViewMatcher("Group " + tabOrTabs),
                            itemDataMatcher(R.id.tab_list_editor_group_menu_item),
                            this::doGroupTabs);
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
     * @return the "New tab group" dialog as a Facility.
     */
    public NewTabGroupDialogFacility groupTabs() {
        assert ChromeFeatureList.sTabGroupCreationDialogAndroid.isEnabled();
        return mGroupWithDialogMenuItem.scrollToAndSelect();
    }

    /** Factory for the result of {@link #groupTabs()}. */
    private NewTabGroupDialogFacility doGroupTabs(
            ItemOnScreenFacility<NewTabGroupDialogFacility> itemOnScreen) {
        SoftKeyboardFacility softKeyboard =
                new SoftKeyboardFacility(mHostStation.getActivityElement());
        NewTabGroupDialogFacility dialog =
                new NewTabGroupDialogFacility(mListEditor.getAllTabIdsSelected(), softKeyboard);
        mHostStation.swapFacilitiesSync(
                List.of(this, mListEditor, itemOnScreen),
                List.of(dialog, softKeyboard),
                itemOnScreen.clickTrigger());
        return dialog;
    }

    /**
     * Select "Group tabs" to create a new group with the selected tabs, expecting no dialog.
     *
     * <p>The tab creation dialog does not appear when at least one group is selected (one of the
     * groups will be extended instead of a new group being created).
     *
     * @return the new group card and the undo snackbar expected to be shown.
     */
    public Pair<TabSwitcherGroupCardFacility, UndoGroupSnackbarFacility> groupTabsWithoutDialog() {
        assert mListEditor.isAnyGroupSelected();
        return mGroupWithoutDialogMenuItem.scrollToAndSelect();
    }

    /** Factory for the result of {@link #groupTabsWithoutDialog()}. */
    private Pair<TabSwitcherGroupCardFacility, UndoGroupSnackbarFacility> doGroupTabsWithoutDialog(
            ItemOnScreenFacility<Pair<TabSwitcherGroupCardFacility, UndoGroupSnackbarFacility>>
                    itemOnScreen) {
        List<Integer> tabIdsSelected = mListEditor.getAllTabIdsSelected();
        String title = TabGroupUtil.getNumberOfTabsString(tabIdsSelected.size());
        String snackbarMessage = TabGroupUtil.getSnackbarMessageString(tabIdsSelected.size());
        var card = new TabSwitcherGroupCardFacility(/* cardIndex= */ null, tabIdsSelected, title);
        var undoSnackbar = new UndoGroupSnackbarFacility(snackbarMessage);
        mHostStation.swapFacilitiesSync(
                List.of(this, mListEditor, itemOnScreen),
                List.of(card, undoSnackbar),
                itemOnScreen.clickTrigger());
        return Pair.create(card, undoSnackbar);
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
