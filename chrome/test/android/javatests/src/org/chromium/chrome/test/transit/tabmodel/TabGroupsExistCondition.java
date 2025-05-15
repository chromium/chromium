// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/** Check that this list of tab group matches the expected list of tab groups. */
public class TabGroupsExistCondition extends UiThreadCondition {

    private final boolean mIncognito;
    private final List<Token> mExpectedTabGroups;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;

    public TabGroupsExistCondition(
            boolean incognito,
            List<Token> expectedTabGroups,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        mIncognito = incognito;
        mExpectedTabGroups = expectedTabGroups;
        mTabModelSelectorSupplier = dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        TabGroupModelFilter groupFilter =
                mTabModelSelectorSupplier
                        .get()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(mIncognito);
        Set<Token> allTabGroupIds = groupFilter.getAllTabGroupIds();
        if (!matchExpectedTabGroups(allTabGroupIds, mExpectedTabGroups)) {
            return notFulfilled(
                    "Expected tab groups are different to actual. Expected: %s, Actual: %s",
                    tabGroupIdCollectionToString(mExpectedTabGroups),
                    tabGroupIdCollectionToString(allTabGroupIds));
        }
        return fulfilled();
    }

    private static boolean matchExpectedTabGroups(
            Set<Token> allTabGroupIds, List<Token> expectedTabGroups) {
        for (Token tabGroupId : expectedTabGroups) {
            if (!allTabGroupIds.contains(tabGroupId)) return false;
        }
        return expectedTabGroups.size() == allTabGroupIds.size();
    }

    @Override
    public String buildDescription() {
        return String.format(
                "Checking whether expected %s tab groups match actual: %s",
                mIncognito ? "Incognito" : "Regular",
                tabGroupIdCollectionToString(mExpectedTabGroups));
    }

    private String tabGroupIdCollectionToString(Collection<Token> tabGroupIds) {
        StringBuilder result = new StringBuilder("[");

        int i = 0;
        int collectionSize = tabGroupIds.size();
        Iterator<Token> iterator = tabGroupIds.iterator();

        // Stop before the final element.
        while (i < collectionSize - 2) {
            result.append(iterator.next()).append(", ");
            i++;
        }

        // Append the last element without a following comma.
        if (iterator.hasNext()) {
            result.append(iterator.next());
        }

        result.append("]");
        return result.toString();
    }
}
