// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.chromium.base.test.transit.Condition.whetherEquals;
import static org.chromium.base.test.transit.SimpleConditions.uiThreadCondition;

import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.tabmodel.TabGroupExistsCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Represents a tab group card in the Tab Switcher.
 *
 * <p>TODO(crbug.com/340913718): Amend the card Matcher<View> to include the expected background
 * color depending on if it's focused. Requires the ViewElement to only be generated after the
 * ActivityElement is matched to an Activity because the Activity needs to be used as context to get
 * the expected background color to build the matcher.
 */
public class TabSwitcherGroupCardFacility extends TabSwitcherCardFacility {
    /**
     * Expect the default title "N tabs".
     *
     * <p>Equivalent to using the constructor {@link #TabSwitcherGroupCardFacility(Integer, List)}.
     */
    public static final String DEFAULT_N_TABS_TITLE = "_DEFAULT_N_TABS_TITLE";

    private final List<Integer> mTabIdsToGroup;
    public ViewElement<View> menuButtonElement;

    public TabSwitcherGroupCardFacility(@Nullable Integer cardIndex, List<Integer> tabIdsToGroup) {
        this(cardIndex, tabIdsToGroup, DEFAULT_N_TABS_TITLE);
    }

    public TabSwitcherGroupCardFacility(@Nullable Integer cardIndex, List<Integer> tabIdsToGroup, String title) {
        super(
                cardIndex,
                title.equals(DEFAULT_N_TABS_TITLE) || title.isEmpty()
                        ? TabGroupUtil.getNumberOfTabsString(tabIdsToGroup.size())
                        : title);
        assert !tabIdsToGroup.isEmpty();

        mTabIdsToGroup = new ArrayList<>(tabIdsToGroup);
        Collections.sort(mTabIdsToGroup);
    }

    @Override
    public void declareExtraElements() {
        super.declareExtraElements();
        menuButtonElement = declareActionButton();

        declareEnterCondition(
                new TabGroupExistsCondition(
                        mHostStation.tabGroupModelFilterElement, mTabIdsToGroup));
    }

    /** Clicks the group card to open the tab group dialog. */
    public TabGroupDialogFacility<TabSwitcherStation> clickCard() {
        return titleElement.clickTo().enterFacility(new TabGroupDialogFacility<>(mTabIdsToGroup));
    }

    /** Clicks the ("...") action button on a tab group to open the overflow menu. */
    public TabSwitcherGroupCardAppMenuFacility openAppMenu() {
        boolean isIncognito = mHostStation.isIncognito();
        return menuButtonElement
                .clickTo()
                .enterFacility(new TabSwitcherGroupCardAppMenuFacility(isIncognito, mTitle));
    }

    /**
     * Waits for the tab group card to have a specific color.
     *
     * @param color The expected {@link TabGroupColorId}.
     * @return The {@link TabGroupCardColorFacility} for the given color.
     */
    public TabGroupCardColorFacility expectColor(@TabGroupColorId int color) {
        return noopTo().enterFacility(new TabGroupCardColorFacility(color));
    }

    public class TabGroupCardColorFacility extends Facility<TabSwitcherStation> {
        public final ViewElement<FrameLayout> colorViewElement;

        public TabGroupCardColorFacility(@TabGroupColorId int color) {
            ColorStateList expectedColor =
                    ColorStateList.valueOf(
                            TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                                    TabSwitcherGroupCardFacility.this.mHostStation.getActivity(),
                                    color,
                                    false));

            ViewSpec<View> colorContainerSpec =
                    cardViewElement.descendant(withId(R.id.tab_group_color_view_container));
            colorViewElement =
                    declareView(FrameLayout.class, withParent(colorContainerSpec.getViewMatcher()));
            declareEnterCondition(
                    uiThreadCondition(
                            "Tab group color should be " + expectedColor,
                            colorViewElement,
                            colorView -> {
                                return whetherEquals(
                                        expectedColor,
                                        ((GradientDrawable) colorView.getBackground()).getColor());
                            }));
        }
    }
}
