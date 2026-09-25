// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.HashSet;
import java.util.Set;
import java.util.function.Supplier;

/** Checks that one new tab group was created. */
@NullMarked
public class TabGroupCreatedCondition extends ConditionWithResult<Token> {
    private final Supplier<TabModel> mTabModelSupplier;
    private @Nullable Set<Token> mOriginalTabGroupIds;

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
        if (mOriginalTabGroupIds == null) {
            return notFulfilled().withoutResult();
        }

        Set<Token> newTabGroupIds = new HashSet<>(mTabModelSupplier.get().getAllTabGroupIds());
        newTabGroupIds.removeAll(mOriginalTabGroupIds);

        int changeInTabGroupCount = newTabGroupIds.size();
        if (changeInTabGroupCount != 1) {
            return notFulfilled(
                            "Incorrect change in number of tab groups: Expected 1, Actual %d.",
                            changeInTabGroupCount)
                    .withoutResult();
        }
        Token newGroupId = newTabGroupIds.iterator().next();
        return fulfilled().withResult(newGroupId);
    }

    @Override
    public String buildDescription() {
        return String.format("Tab group created");
    }
}
