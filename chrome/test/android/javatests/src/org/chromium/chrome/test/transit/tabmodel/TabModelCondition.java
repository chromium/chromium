// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.function.Supplier;

/** Condition fulfilled when an initialized TabModel exists. */
public class TabModelCondition extends ConditionWithResult<TabModel> {
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final boolean mIncognito;

    public TabModelCondition(
            Supplier<TabModelSelector> tabModelSelectorSupplier, boolean incognito) {
        super(/* isRunOnUiThread= */ true);
        mTabModelSelectorSupplier = dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
        mIncognito = incognito;
    }

    @Override
    protected ConditionStatusWithResult<TabModel> resolveWithSuppliers() throws Exception {
        TabModel model = mTabModelSelectorSupplier.get().getModel(mIncognito);
        return fulfilled("%d tabs", model.getCount()).withResult(model);
    }

    @Override
    public String buildDescription() {
        return (mIncognito ? "Incognito " : "Regular") + " TabModel is available";
    }
}
