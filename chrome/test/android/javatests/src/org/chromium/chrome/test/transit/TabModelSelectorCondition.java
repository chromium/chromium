// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Condition fulfilled when an initialized TabModel exists. */
public class TabModelSelectorCondition extends UiThreadCondition
        implements Supplier<TabModelSelector> {

    private final Supplier<ChromeTabbedActivity> mActivitySupplier;
    private TabModelSelector mSelector;

    public TabModelSelectorCondition(Supplier<ChromeTabbedActivity> activitySupplier) {
        mActivitySupplier = activitySupplier;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        TabModelSelector selector = mActivitySupplier.get().getTabModelSelectorSupplier().get();
        if (selector != null) {
            mSelector = selector;
            return fulfilled();
        } else {
            return awaiting("Activity has no TabModelSelector");
        }
    }

    @Override
    public String buildDescription() {
        return null;
    }

    @Override
    public TabModelSelector get() {
        return mSelector;
    }
}
