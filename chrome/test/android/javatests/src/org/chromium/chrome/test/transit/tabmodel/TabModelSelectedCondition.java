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

/** Fulfilled when the TabModelSelector has the incognito or the regular TabModel selected. */
@NullMarked
public class TabModelSelectedCondition extends Condition {
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final boolean mIncognito;

    public TabModelSelectedCondition(
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier, boolean incognito) {
        super(/* isRunOnUiThread= */ true);
        mTabModelSelectorSupplier = dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
        mIncognito = incognito;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        boolean incognitoSelected =
                assumeNonNull(mTabModelSelectorSupplier.get()).isIncognitoSelected();
        return whether(
                incognitoSelected == mIncognito, "isIncognitoSelected=%b", incognitoSelected);
    }

    @Override
    public String buildDescription() {
        return (mIncognito ? "Incognito" : "Regular") + " TabModel selected";
    }
}
