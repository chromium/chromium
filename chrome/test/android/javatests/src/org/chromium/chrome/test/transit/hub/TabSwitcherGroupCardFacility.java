// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import androidx.annotation.Nullable;
import org.chromium.base.test.transit.Elements;
import org.chromium.chrome.test.transit.tabmodel.TabGroupExistsCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;

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

    public TabSwitcherGroupCardFacility(@Nullable Integer cardIndex, List<Integer> tabIdsToGroup) {
        this(cardIndex, tabIdsToGroup, DEFAULT_N_TABS_TITLE);
    }

    public TabSwitcherGroupCardFacility(@Nullable Integer cardIndex, List<Integer> tabIdsToGroup, String title) {
        super(
                cardIndex,
                title.equals(DEFAULT_N_TABS_TITLE)
                        ? TabGroupUtil.getNumberOfTabsString(tabIdsToGroup.size())
                        : title);
        assert !tabIdsToGroup.isEmpty();

        mTabIdsToGroup = new ArrayList<>(tabIdsToGroup);
        Collections.sort(mTabIdsToGroup);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareEnterCondition(
                new TabGroupExistsCondition(
                        mHostStation.isIncognito(),
                        mTabIdsToGroup,
                        mHostStation.getTabModelSelectorSupplier()));
    }

    /** Clicks the group card to open the tab group dialog. */
    public TabGroupDialogFacility<TabSwitcherStation> clickCard() {
        boolean isIncognito = mHostStation.isIncognito();
        return mHostStation.enterFacilitySync(
                new TabGroupDialogFacility<>(mTabIdsToGroup, isIncognito), clickTitleTrigger());
    }
}
