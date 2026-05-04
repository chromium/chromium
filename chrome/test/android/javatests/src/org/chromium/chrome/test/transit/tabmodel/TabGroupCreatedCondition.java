// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import static org.junit.Assert.assertEquals;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.Set;
import java.util.function.Supplier;

/** Checks that one new tab group was created. */
public class TabGroupCreatedCondition extends ConditionWithResult<Token> {
    private final Supplier<TabModel> mTabModelSupplier;
    private Set<Token> mOriginalTabGroupIds;

    public TabGroupCreatedCondition(Supplier<TabModel> tabModelSupplier) {
        super(/* isRunOnUiThread= */ true);
        mTabModelSupplier = dependOnSupplier(tabModelSupplier, "TabModel");
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        TabModel tabModel = mTabModelSupplier.get();
        mOriginalTabGroupIds =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getAllTabGroupIds());
    }

    @Override
    protected ConditionStatusWithResult<Token> resolveWithSuppliers() throws Exception {
        Set<Token> newTabGroupIds = mTabModelSupplier.get().getAllTabGroupIds();
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
