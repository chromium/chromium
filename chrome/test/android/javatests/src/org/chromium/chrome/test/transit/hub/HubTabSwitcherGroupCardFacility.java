// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.HubTabSwitcherBaseStation;
import org.chromium.chrome.test.transit.tab_groups.TabGroupExistsCondition;
import org.chromium.chrome.test.transit.tab_groups.TabGroupUtil;

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
public class HubTabSwitcherGroupCardFacility extends Facility<HubTabSwitcherBaseStation> {
    public static final Matcher<View> CARD = withId(R.id.card_view);

    private final List<Integer> mTabIdsToGroup;
    private final String mTitle;

    public HubTabSwitcherGroupCardFacility(
            HubTabSwitcherBaseStation station, List<Integer> tabIdsToGroup) {
        this(station, tabIdsToGroup, TabGroupUtil.getNumberOfTabsString(tabIdsToGroup.size()));
    }

    public HubTabSwitcherGroupCardFacility(
            HubTabSwitcherBaseStation station, List<Integer> tabIdsToGroup, String title) {
        super(station);
        assert !tabIdsToGroup.isEmpty();

        mTabIdsToGroup = new ArrayList<>(tabIdsToGroup);
        Collections.sort(mTabIdsToGroup);
        mTitle = title;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(
                ViewElement.sharedViewElement(
                        allOf(withText(mTitle), withId(R.id.tab_title), withParent(CARD))));

        elements.declareEnterCondition(
                new TabGroupExistsCondition(
                        mHostStation.isIncognito(),
                        mTabIdsToGroup,
                        mHostStation.getTabModelSelectorSupplier()));
    }
}
