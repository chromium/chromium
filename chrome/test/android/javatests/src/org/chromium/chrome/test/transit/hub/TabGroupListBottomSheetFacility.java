// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.BottomSheetFacility;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.tabmodel.TabGroupsExistCondition;

import java.util.List;

/**
 * Bottom Sheet that appears to move a tab or list of tabs to a tab group.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class TabGroupListBottomSheetFacility<HostStationT extends Station<ChromeTabbedActivity>>
        extends BottomSheetFacility<HostStationT> {
    private final List<Token> mTabGroupIds;
    public final ViewElement<View> titleElement;
    public final ViewElement<RecyclerView> recyclerViewElement;
    public final ViewElement<View> newTabGroupRow;

    /** Constructor. Expects a specific title and selected color. */
    public TabGroupListBottomSheetFacility(
            List<Token> expectedTabGroupIds, boolean isNewTabGroupRowVisible) {
        assertTrue(ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled());

        mTabGroupIds = expectedTabGroupIds;

        // Declare the drag handlebar.
        declareDescendantView(
                ImageView.class, withId(R.id.tab_group_list_bottom_sheet_drag_handlebar));

        titleElement =
                declareDescendantView(
                        withId(R.id.tab_group_parity_bottom_sheet_title_text), withText("Add to"));
        recyclerViewElement =
                declareDescendantView(
                        RecyclerView.class, withId(R.id.tab_group_parity_recycler_view));

        // If `isNewTabGroupRowVisible` is true, the new tab group row should always be visible on
        // bottom sheet showing.
        if (isNewTabGroupRowVisible) {
            newTabGroupRow =
                    declareView(
                            recyclerViewElement.descendant(
                                    withId(R.id.tab_group_list_new_group_row),
                                    hasDescendant(withText("New tab group"))));
        } else {
            newTabGroupRow = null;
        }
    }

    @Override
    public void declareExtraElements() {
        ChromeTabbedActivity activity = mHostStation.getActivity();
        boolean isIncognito = activity.getCurrentTabModel().isIncognitoBranded();
        Supplier<TabModelSelector> tabModelSelectorSupplier =
                activity.getTabModelSelectorSupplier();
        declareEnterCondition(
                new TabGroupsExistCondition(isIncognito, mTabGroupIds, tabModelSelectorSupplier));
    }

    /** Clicks the "New tab group" row to initialize the UI flow for creating a new tab group. */
    public NewTabGroupDialogFacility<HostStationT> clickNewTabGroupRow() {
        assert newTabGroupRow != null : "New tab group row was not expected to be visible.";

        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        NewTabGroupDialogFacility<HostStationT> newTabGroupDialog =
                new NewTabGroupDialogFacility<>(softKeyboard);
        mHostStation.swapFacilitiesSync(
                List.of(this),
                List.of(newTabGroupDialog, softKeyboard),
                newTabGroupRow.getClickTrigger());
        return newTabGroupDialog;
    }
}
