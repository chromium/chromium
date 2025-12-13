// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import static org.junit.Assert.assertEquals;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;

import java.util.Set;
import java.util.function.Supplier;

/** Checks that one new tab group was created. */
public class TabGroupCreatedCondition extends ConditionWithResult<Token> {
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private Set<Token> mOriginalTabGroupIds;

    public TabGroupCreatedCondition(Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        super(/* isRunOnUiThread= */ true);
        mTabGroupModelFilterSupplier =
                dependOnSupplier(tabGroupModelFilterSupplier, "TabGroupModelFilter");
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        TabGroupModelFilter tabGroupModelFilter = mTabGroupModelFilterSupplier.get();
        mOriginalTabGroupIds =
                ThreadUtils.runOnUiThreadBlocking(() -> tabGroupModelFilter.getAllTabGroupIds());
    }

    @Override
    protected ConditionStatusWithResult<Token> resolveWithSuppliers() throws Exception {
        Set<Token> newTabGroupIds = mTabGroupModelFilterSupplier.get().getAllTabGroupIds();
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
        return String.format("Tab group created");
    }
}
