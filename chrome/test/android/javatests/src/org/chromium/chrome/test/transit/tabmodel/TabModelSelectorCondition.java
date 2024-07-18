// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Condition fulfilled when an initialized TabModel exists. */
public class TabModelSelectorCondition extends ConditionWithResult<TabModelSelector> {

    private final Supplier<ChromeTabbedActivity> mActivitySupplier;

    public TabModelSelectorCondition(Supplier<ChromeTabbedActivity> activitySupplier) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
    }

    @Override
    protected ConditionStatusWithResult<TabModelSelector> resolveWithSuppliers() throws Exception {
        TabModelSelector selector = mActivitySupplier.get().getTabModelSelectorSupplier().get();
        if (selector != null) {
            return fulfilled().withResult(selector);
        } else {
            return awaiting("Activity has no TabModelSelector").withoutResult();
        }
    }

    @Override
    public String buildDescription() {
        return "TabModelSelector is available";
    }
}
