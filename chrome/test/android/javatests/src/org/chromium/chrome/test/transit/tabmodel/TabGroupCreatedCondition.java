// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import static org.junit.Assert.assertEquals;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Set;

/** Checks that one new tab group was created. */
public class TabGroupCreatedCondition extends ConditionWithResult<Token> {
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final boolean mIncognito;
    private Set<Token> mOriginalTabGroupIds;

    public TabGroupCreatedCondition(
            boolean incognito, Supplier<TabModelSelector> tabModelSelectorSupplier) {
        super(/* isRunOnUiThread= */ true);
        mTabModelSelectorSupplier = dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
        mIncognito = incognito;
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        TabGroupModelFilter tabGroupModelFilter = getTabGroupModelFilter();
        mOriginalTabGroupIds = tabGroupModelFilter.getAllTabGroupIds();
    }

    @Override
    protected ConditionStatusWithResult<Token> resolveWithSuppliers() throws Exception {
        TabGroupModelFilter tabGroupModelFilter = getTabGroupModelFilter();
        Set<Token> newTabGroupIds = tabGroupModelFilter.getAllTabGroupIds();
        newTabGroupIds.removeAll(mOriginalTabGroupIds);

        int changeInTabGroupCount = newTabGroupIds.size();
        assertEquals(1, changeInTabGroupCount);
        if (changeInTabGroupCount != 1) {
            return notFulfilled(
                            "Incorrect change in number of tab groups: Expected 1, Actual %d.",
                            changeInTabGroupCount)
                    .withResult(null);
        }
        Token newGroupId = newTabGroupIds.iterator().next();
        return fulfilled().withResult(newGroupId);
    }

    @Override
    public String buildDescription() {
        return "Tab group model filter to be monitored for tab group creation: " + mIncognito;
    }

    private TabGroupModelFilter getTabGroupModelFilter() {
        return mTabModelSelectorSupplier
                .get()
                .getTabGroupModelFilterProvider()
                .getTabGroupModelFilter(mIncognito);
    }
}
