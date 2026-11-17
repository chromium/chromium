// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.function.Supplier;

/** Fulfilled when a TabModel contains the Tab with a given id, e.g. after moving it there. */
@NullMarked
public class TabInModelCondition extends Condition {
    private final int mTabId;
    private final Supplier<@Nullable TabModel> mTabModelSupplier;

    public TabInModelCondition(int tabId, Supplier<@Nullable TabModel> tabModelSupplier) {
        super(/* isRunOnUiThread= */ true);
        mTabId = tabId;
        mTabModelSupplier = dependOnSupplier(tabModelSupplier, "TabModel");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        TabModel model = assumeNonNull(mTabModelSupplier.get());
        return whether(model.getTabById(mTabId) != null, "TabModel has %d tabs", model.getCount());
    }

    @Override
    public String buildDescription() {
        return "Tab " + mTabId + " is in TabModel";
    }
}
