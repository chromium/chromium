// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.function.Supplier;

/** Condition fulfilled when an initialized TabModel exists. */
public class TabGroupModelFilterCondition extends ConditionWithResult<TabGroupModelFilter> {
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final boolean mIncognito;

    public TabGroupModelFilterCondition(
            Supplier<TabModelSelector> tabModelSelectorSupplier, boolean incognito) {
        super(/* isRunOnUiThread= */ true);
        mTabModelSelectorSupplier = dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
        mIncognito = incognito;
    }

    @Override
    protected ConditionStatusWithResult<TabGroupModelFilter> resolveWithSuppliers()
            throws Exception {
        TabGroupModelFilter model =
                mTabModelSelectorSupplier
                        .get()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(mIncognito);
        if (model == null) {
            return notFulfilled("TabGroupModelFilter is null").withoutResult();
        }
        return fulfilled("%d groups", model.getTabGroupCount()).withResult(model);
    }

    @Override
    public String buildDescription() {
        return (mIncognito ? "Incognito " : "Regular") + " TabGroupModelFilter is available";
    }
}
