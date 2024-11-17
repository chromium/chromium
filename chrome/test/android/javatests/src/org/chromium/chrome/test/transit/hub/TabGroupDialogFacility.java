// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;
import android.widget.EditText;

import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;

/**
 * Dialog that appears when a tab group is clicked on in the Tab Switcher or when the tab group
 * snackbar is expanded.
 *
 * @param <HostStationT> the station where the Tab Group Dialog is opened from. Should be
 *     TabSwitcherStation or PageStation.
 */
public class TabGroupDialogFacility<HostStationT extends Station<ChromeTabbedActivity>>
        extends Facility<HostStationT> {
    public static final Matcher<View> TOOLBAR_MATCHER =
            isDescendantOfA(withId(R.id.tab_group_toolbar));
    public static final Matcher<View> TITLE_INPUT_MATCHER =
            allOf(withId(R.id.title), isAssignableFrom(EditText.class), TOOLBAR_MATCHER);

    public static final ViewSpec TABS_LIST =
            viewSpec(
                    withId(R.id.tab_list_recycler_view),
                    withParent(withId(R.id.tab_grid_dialog_recycler_view_container)));
    public static final ViewSpec COLOR_ICON =
            viewSpec(withId(R.id.tab_group_color_icon_container), TOOLBAR_MATCHER);
    public static final ViewSpec NEW_TAB_BUTTON =
            viewSpec(withId(R.id.toolbar_new_tab_button), TOOLBAR_MATCHER);
    public static final ViewSpec BACK_BUTTON =
            viewSpec(withId(R.id.toolbar_back_button), TOOLBAR_MATCHER);
    public static final ViewSpec LIST_MENU_BUTTON = viewSpec(withId(R.id.toolbar_menu_button));

    private final List<Integer> mTabIdsInGroup;
    private final String mTitle;
    private final ViewSpec mTitleInputSpec;
    private final boolean mIsIncognito;
    private final @Nullable @TabGroupColorId Integer mSelectedColor;

    /**
     * Constructor. The expected title is "n tabs", where n is the number of tabs in the group.
     * Expects no particular color.
     */
    public TabGroupDialogFacility(List<Integer> tabIdsInGroup, boolean isIncognito) {
        this(
                tabIdsInGroup,
                TabGroupUtil.getNumberOfTabsString(tabIdsInGroup.size()),
                /* selectedColor= */ null,
                isIncognito);
    }

    /** Constructor. Expects a specific title and selected color. */
    public TabGroupDialogFacility(
            List<Integer> tabIdsInGroup,
            String title,
            @Nullable @TabGroupColorId Integer selectedColor,
            boolean isIncognito) {
        mTabIdsInGroup = tabIdsInGroup;
        mTitle = title;
        mSelectedColor = selectedColor;
        mIsIncognito = isIncognito;

        mTitleInputSpec = viewSpec(withText(mTitle), TITLE_INPUT_MATCHER);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(TABS_LIST);
        elements.declareView(COLOR_ICON);
        elements.declareView(mTitleInputSpec);
        elements.declareView(NEW_TAB_BUTTON);
        elements.declareView(BACK_BUTTON);
        elements.declareView(LIST_MENU_BUTTON);
    }

    /** Input a new group name. */
    public TabGroupDialogFacility<HostStationT> inputName(String newTabGroupName) {
        return mHostStation.swapFacilitySync(
                this,
                new TabGroupDialogFacility<>(
                        mTabIdsInGroup, newTabGroupName, mSelectedColor, mIsIncognito),
                () -> mTitleInputSpec.perform(replaceText(newTabGroupName)));
    }

    /** Create a new tab and transition to the associated RegularNewTabPageStation. */
    public RegularNewTabPageStation openNewRegularTab() {
        assert !mIsIncognito;

        RegularNewTabPageStation page =
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(page, NEW_TAB_BUTTON::click);
    }

    /** Create a new incognito tab and transition to the associated IncognitoNewTabPageStation. */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        assert mIsIncognito;

        IncognitoNewTabPageStation page =
                IncognitoNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(page, NEW_TAB_BUTTON::click);
    }

    /** Press back to exit the facility. */
    public void pressBackArrowToExit() {
        mHostStation.exitFacilitySync(this, BACK_BUTTON::click);
    }
}
