// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.util.Pair;
import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.CtaAppMenuFacility;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;
import org.chromium.ui.modelutil.MVCListAdapter;

import java.util.List;

/**
 * App menu shown when in the "Select Tabs" state in the Hub Tab Switcher.
 *
 * <p>Differs significantly from the app menu normally shown; the options are operations to change
 * the tab selection or to do something with the selected tabs.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public class TabListEditorAppMenu<HostStationT extends TabSwitcherStation>
        extends CtaAppMenuFacility<HostStationT> {

    private final TabSwitcherListEditorFacility<HostStationT> mListEditor;
    private Item<Void> mCloseMenuItem;
    private Item<NewTabGroupDialogFacility<HostStationT>> mGroupWithDialogMenuItem;
    private Item<Pair<TabSwitcherGroupCardFacility, UndoSnackbarFacility<HostStationT>>>
            mGroupWithoutDialogMenuItem;

    public TabListEditorAppMenu(TabSwitcherListEditorFacility<HostStationT> listEditor) {
        mListEditor = listEditor;
    }

    @Override
    protected void declareItems(ScrollableFacility<HostStationT>.ItemsBuilder items) {
        String tabOrTabs = mListEditor.getNumTabsSelected() > 1 ? "tabs" : "tab";

        // "Select all" usually, or "Deselect all" if all tabs are selected.
        items.declarePossibleStubItem();

        mCloseMenuItem =
                items.declareItem(
                        itemViewSpec(withText("Close " + tabOrTabs)),
                        itemDataMatcher(R.id.tab_list_editor_close_menu_item),
                        this::doCloseTabs);

        // "Group tab(s)" or "Add tab(s) to new group"
        ViewSpec<View> groupTabsViewSpec;
        Matcher<MVCListAdapter.ListItem> groupTabsDataMatcher;
        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            groupTabsViewSpec =
                    itemViewSpec(withText(String.format("Add %s to new group", tabOrTabs)));
            groupTabsDataMatcher = itemDataMatcher(R.id.tab_list_editor_add_tab_to_group_menu_item);
            if (mListEditor.isAnyGroupSelected()) {
                throw new UnsupportedOperationException(
                        "Bottom sheet tab group merging not supported yet");
            } else {
                mGroupWithDialogMenuItem =
                        items.declareItem(
                                groupTabsViewSpec, groupTabsDataMatcher, this::doGroupTabs);
            }
        } else {
            groupTabsViewSpec = itemViewSpec(withText("Group " + tabOrTabs));
            groupTabsDataMatcher = itemDataMatcher(R.id.tab_list_editor_group_menu_item);
            if (mListEditor.isAnyGroupSelected()) {
                mGroupWithoutDialogMenuItem =
                        items.declareItem(
                                groupTabsViewSpec,
                                groupTabsDataMatcher,
                                this::doGroupTabsWithoutDialog);
            } else {
                mGroupWithDialogMenuItem =
                        items.declareItem(
                                groupTabsViewSpec, groupTabsDataMatcher, this::doGroupTabs);
            }
        }

        items.declareStubItem(
                itemViewSpec(withText("Bookmark " + tabOrTabs)),
                itemDataMatcher(R.id.tab_list_editor_bookmark_menu_item));

        items.declareStubItem(
                itemViewSpec(withText("Share " + tabOrTabs)),
                itemDataMatcher(R.id.tab_list_editor_share_menu_item));
    }

    /**
     * Select "Group tabs" to create a new group with the selected tabs.
     *
     * @return the "New tab group" dialog as a Facility.
     */
    public NewTabGroupDialogFacility<HostStationT> groupTabs() {
        return mGroupWithDialogMenuItem.scrollToAndSelect();
    }

    /** Factory for the result of {@link #groupTabs()}. */
    private NewTabGroupDialogFacility<HostStationT> doGroupTabs(
            ItemOnScreenFacility<NewTabGroupDialogFacility<HostStationT>> itemOnScreen) {
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        NewTabGroupDialogFacility<HostStationT> dialog =
                new NewTabGroupDialogFacility<>(mListEditor.getAllTabIdsSelected(), softKeyboard);
        mHostStation.swapFacilitiesSync(
                List.of(this, mListEditor, itemOnScreen),
                List.of(dialog, softKeyboard),
                itemOnScreen.viewElement.getClickTrigger());
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
    public Pair<TabSwitcherGroupCardFacility, UndoSnackbarFacility<HostStationT>>
            groupTabsWithoutDialog() {
        assert mListEditor.isAnyGroupSelected();
        return mGroupWithoutDialogMenuItem.scrollToAndSelect();
    }

    /** Factory for the result of {@link #groupTabsWithoutDialog()}. */
    private Pair<TabSwitcherGroupCardFacility, UndoSnackbarFacility<HostStationT>>
            doGroupTabsWithoutDialog(
                    ItemOnScreenFacility<
                                    Pair<
                                            TabSwitcherGroupCardFacility,
                                            UndoSnackbarFacility<HostStationT>>>
                            itemOnScreen) {
        List<Integer> tabIdsSelected = mListEditor.getAllTabIdsSelected();
        String title = TabGroupUtil.getNumberOfTabsString(tabIdsSelected.size());
        String snackbarMessage =
                TabGroupUtil.getUndoGroupTabsSnackbarMessageString(tabIdsSelected.size());
        var card = new TabSwitcherGroupCardFacility(/* cardIndex= */ null, tabIdsSelected, title);
        UndoSnackbarFacility<HostStationT> undoSnackbar =
                new UndoSnackbarFacility<>(snackbarMessage);
        mHostStation.swapFacilitiesSync(
                List.of(this, mListEditor, itemOnScreen),
                List.of(card, undoSnackbar),
                itemOnScreen.viewElement.getClickTrigger());
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
                mHostStation.tabModelSelectorElement.get().getModel(mHostStation.isIncognito());
        Condition tabCountDecreased =
                new TabCountChangedCondition(tabModel, -mListEditor.getNumTabsSelected());
        mHostStation.exitFacilitiesSync(
                List.of(this, mListEditor, itemOnScreen),
                Transition.conditionOption(tabCountDecreased),
                itemOnScreen.viewElement.getClickTrigger());

        return null;
    }
}
