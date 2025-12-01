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
    private Item mCloseMenuItem;
    private Item mGroupOrAddTabsMenuItem;
    private Item mPinMenuItem;

    public TabListEditorAppMenu(TabSwitcherListEditorFacility<HostStationT> listEditor) {
        mListEditor = listEditor;
    }

    @Override
    protected void declareItems(ScrollableFacility<HostStationT>.ItemsBuilder items) {
        String tabOrTabs = mListEditor.getNumTabsSelected() > 1 ? "tabs" : "tab";

        // "Select all" usually, or "Deselect all" if all tabs are selected.
        items.declarePossibleStubItem();

        ViewSpec<? extends View> onScreenViewSpec2 = itemViewSpec(withText("Close " + tabOrTabs));
        mCloseMenuItem =
                items.declareItem(
                        onScreenViewSpec2, itemDataMatcher(R.id.tab_list_editor_close_menu_item));

        // "Group tab(s)" or "Add tab(s) to new group"
        ViewSpec<View> groupTabsViewSpec;
        Matcher<MVCListAdapter.ListItem> groupTabsDataMatcher;
        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            groupTabsViewSpec =
                    itemViewSpec(withText(String.format("Add %s to new group", tabOrTabs)));
            groupTabsDataMatcher = itemDataMatcher(R.id.tab_list_editor_add_tab_to_group_menu_item);
        } else {
            groupTabsViewSpec = itemViewSpec(withText("Group " + tabOrTabs));
            groupTabsDataMatcher = itemDataMatcher(R.id.tab_list_editor_group_menu_item);
        }
        mGroupOrAddTabsMenuItem = items.declareItem(groupTabsViewSpec, groupTabsDataMatcher);

        ViewSpec<? extends View> onScreenViewSpec1 =
                itemViewSpec(withText("Bookmark " + tabOrTabs));
        items.declareItem(
                onScreenViewSpec1, itemDataMatcher(R.id.tab_list_editor_bookmark_menu_item));

        ViewSpec<? extends View> onScreenViewSpec3 = itemViewSpec(withText("Pin " + tabOrTabs));
        mPinMenuItem =
                items.declareItem(
                        onScreenViewSpec3, itemDataMatcher(R.id.tab_list_editor_pin_menu_item));

        ViewSpec<? extends View> onScreenViewSpec = itemViewSpec(withText("Share " + tabOrTabs));
        items.declareItem(onScreenViewSpec, itemDataMatcher(R.id.tab_list_editor_share_menu_item));
    }

    /**
     * Select "Group tabs" to create a new group with the selected tabs.
     *
     * @return the "New tab group" dialog as a Facility.
     */
    public NewTabGroupDialogFacility<HostStationT> groupTabs() {
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        NewTabGroupDialogFacility<HostStationT> dialog =
                new NewTabGroupDialogFacility<>(mListEditor.getAllTabIdsSelected(), softKeyboard);
        return mGroupOrAddTabsMenuItem
                .scrollToAndSelectTo()
                .exitFacilityAnd(mListEditor)
                .enterFacilityAnd(softKeyboard)
                .enterFacility(dialog);
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

        boolean isTabGroupParityBottomSheetEnabled =
                ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled();
        assert !isTabGroupParityBottomSheetEnabled
                : "Bottom sheet tab group merging not supported yet";

        List<Integer> tabIdsSelected = mListEditor.getAllTabIdsSelected();
        String title = TabGroupUtil.getNumberOfTabsString(tabIdsSelected.size());
        String snackbarMessage =
                TabGroupUtil.getUndoGroupTabsSnackbarMessageString(tabIdsSelected.size());
        var card = new TabSwitcherGroupCardFacility(/* cardIndex= */ null, tabIdsSelected, title);
        UndoSnackbarFacility<HostStationT> undoSnackbar =
                new UndoSnackbarFacility<>(snackbarMessage);

        mGroupOrAddTabsMenuItem
                .scrollToAndSelectTo()
                .exitFacilityAnd(mListEditor)
                .enterFacilities(card, undoSnackbar);
        return Pair.create(card, undoSnackbar);
    }

    /** Select "Pin tabs". */
    public void pinTabs() {
        mPinMenuItem.scrollToAndSelectTo().exitFacility(mListEditor);
    }

    /** Select "Close tabs" to close all selected tabs. */
    public void closeTabs() {
        TabModel tabModel = mHostStation.tabModelElement.value();
        Condition tabCountDecreased =
                new TabCountChangedCondition(tabModel, -mListEditor.getNumTabsSelected());
        mCloseMenuItem
                .scrollToAndSelectTo()
                .exitFacilityAnd(mListEditor)
                .waitFor(tabCountDecreased);
    }
}
