// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;
import android.widget.EditText;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
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
    public static final ViewSpec<View> TABS_LIST =
            viewSpec(
                    withId(R.id.tab_list_recycler_view),
                    withParent(withId(R.id.tab_grid_dialog_recycler_view_container)));

    private final List<Integer> mTabIdsInGroup;
    private final String mTitle;
    private final boolean mIsIncognito;
    private final @Nullable @TabGroupColorId Integer mSelectedColor;
    public ViewElement<View> toolbarElement;
    public ViewElement<View> shareButtonElement;
    public ViewElement<View> tabsListElement;
    public ViewElement<View> colorIconElement;
    public ViewElement<View> titleInputElement;
    public ViewElement<View> newTabButtonElement;
    public ViewElement<View> backButtonElement;
    public ViewElement<View> listMenuButtonElement;

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

        toolbarElement = declareView(withId(R.id.tab_group_toolbar));
        tabsListElement = declareView(TABS_LIST);
        colorIconElement =
                declareView(toolbarElement.descendant(withId(R.id.tab_group_color_icon_container)));
        titleInputElement =
                declareView(
                        toolbarElement.descendant(
                                withId(R.id.title),
                                isAssignableFrom(EditText.class),
                                withText(mTitle)));
        newTabButtonElement =
                declareView(toolbarElement.descendant(withId(R.id.toolbar_new_tab_button)));
        backButtonElement =
                declareView(toolbarElement.descendant(withId(R.id.toolbar_back_button)));
    }

    @Override
    public void declareExtraElements() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
            // TODO(ckitagawa): Add handling for an already shared group.
            if (isAllowedToShare()) {
                shareButtonElement =
                        declareView(toolbarElement.descendant(withId(R.id.share_button)));
            }

            // Data sharing layout causes the menu button to be hidden due to the rounded corner.
            listMenuButtonElement =
                    declareView(
                            toolbarElement.descendant(withId(R.id.toolbar_menu_button)),
                            ViewElement.displayingAtLeastOption(51));
        } else {
            listMenuButtonElement =
                    declareView(toolbarElement.descendant(withId(R.id.toolbar_menu_button)));
        }
    }

    private boolean isAllowedToShare() {
        if (mIsIncognito) return false;

        Profile profile =
                mHostStation
                        .getActivity()
                        .getTabModelSelector()
                        .getModel(mIsIncognito)
                        .getProfile();
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        CollaborationServiceFactory.getForProfile(profile)
                                .getServiceStatus()
                                .isAllowedToCreate());
    }

    /** Input a new group name. */
    public TabGroupDialogFacility<HostStationT> inputName(String newTabGroupName) {
        return mHostStation.swapFacilitySync(
                this,
                new TabGroupDialogFacility<>(
                        mTabIdsInGroup, newTabGroupName, mSelectedColor, mIsIncognito),
                titleInputElement.getPerformTrigger(replaceText(newTabGroupName)));
    }

    /** Create a new tab and transition to the associated RegularNewTabPageStation. */
    public RegularNewTabPageStation openNewRegularTab() {
        assert !mIsIncognito;

        RegularNewTabPageStation page =
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(page, newTabButtonElement.getClickTrigger());
    }

    /** Create a new incognito tab and transition to the associated IncognitoNewTabPageStation. */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        assert mIsIncognito;

        IncognitoNewTabPageStation page =
                IncognitoNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return mHostStation.travelToSync(page, newTabButtonElement.getClickTrigger());
    }

    /** Press back to exit the facility. */
    public void pressBackArrowToExit() {
        mHostStation.exitFacilitySync(this, backButtonElement.getClickTrigger());
    }
}
