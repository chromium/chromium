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

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.HubTabSwitcherBaseStation;

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
    private final int mTabCount;
    private final String mTitle;

    public HubTabSwitcherGroupCardFacility(
            HubTabSwitcherBaseStation station, List<Integer> tabIdsToGroup) {
        super(station);
        assert !tabIdsToGroup.isEmpty();

        mTabIdsToGroup = new ArrayList<>(tabIdsToGroup);
        Collections.sort(mTabIdsToGroup);
        mTabCount = tabIdsToGroup.size();
        mTitle = mTabCount > 1 ? String.format("%d tabs", mTabCount) : "1 tab";
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(
                ViewElement.sharedViewElement(
                        allOf(withText(mTitle), withId(R.id.tab_title), withParent(CARD))));

        elements.declareEnterCondition(
                new TabGroupExists(mHostStation.getTabModelSelectorSupplier()));
    }

    /** Check that the tab group represented by the card exists in the tab model. */
    private class TabGroupExists extends UiThreadCondition {

        private final Supplier<TabModelSelector> mTabModelSelectorSupplier;

        TabGroupExists(Supplier<TabModelSelector> tabModelSelectorSupplier) {
            mTabModelSelectorSupplier = tabModelSelectorSupplier;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            TabGroupModelFilter groupFilter =
                    (TabGroupModelFilter)
                            mTabModelSelectorSupplier
                                    .get()
                                    .getTabModelFilterProvider()
                                    .getTabModelFilter(mHostStation.isIncognito());
            List<Integer> relatedTabIds =
                    new ArrayList<>(groupFilter.getRelatedTabIds(mTabIdsToGroup.get(0)));
            if (relatedTabIds.isEmpty()) {
                return notFulfilled("relatedTabIds is empty");
            }

            Collections.sort(relatedTabIds);
            return whether(
                    mTabIdsToGroup.equals(relatedTabIds), "relatedTabIds: %s", relatedTabIds);
        }

        @Override
        public String buildDescription() {
            return String.format("TabGroup exists with tabIds %s", mTabIdsToGroup);
        }
    }
}
