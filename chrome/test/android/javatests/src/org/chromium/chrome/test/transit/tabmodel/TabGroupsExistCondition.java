// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.Token;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;

import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Check that this list of tab group matches the expected list of tab groups. */
public class TabGroupsExistCondition extends UiThreadCondition {

    private final List<Token> mExpectedTabGroups;
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterProvider;

    public TabGroupsExistCondition(
            List<Token> expectedTabGroups,
            Supplier<TabGroupModelFilter> tabGroupModelFilterProvider) {
        mExpectedTabGroups = expectedTabGroups;
        mTabGroupModelFilterProvider =
                dependOnSupplier(tabGroupModelFilterProvider, "TabGroupModelFilter");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        TabGroupModelFilter groupFilter = mTabGroupModelFilterProvider.get();
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
                "Checking whether expected tab groups match actual: %s",
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
