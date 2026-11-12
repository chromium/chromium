// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.function.Supplier;

/** Fulfilled when the TabModelSelector has finished initializing its tab state. */
@NullMarked
public class TabStateInitializedCondition extends Condition {
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;

    public TabStateInitializedCondition(
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier) {
        super(/* isRunOnUiThread= */ true);
        mTabModelSelectorSupplier = dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        TabModelSelector selector = assumeNonNull(mTabModelSelectorSupplier.get());
        if (selector.isTabStateInitialized()) {
            return fulfilled();
        }
        return awaiting("Tab state is not initialized");
    }

    @Override
    public String buildDescription() {
        return "Tab state is initialized";
    }
}
