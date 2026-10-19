// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.chromium.base.test.transit.Condition.whether;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.tab.Tab;

import java.util.function.Supplier;

/** Condition to check the page title. */
public class PageTitleCondition extends Condition {
    private final String mExpectedTitle;
    private final Supplier<Tab> mLoadedTabSupplier;

    public PageTitleCondition(String expectedTitle, Supplier<Tab> loadedTabSupplier) {
        super(/* isRunOnUiThread= */ true);
        mExpectedTitle = expectedTitle;
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        String title = mLoadedTabSupplier.get().getTitle();
        return whether(mExpectedTitle.equals(title), "ActivityTab title: \"%s\"", title);
    }

    @Override
    public String buildDescription() {
        return String.format("Title of activity tab is \"%s\"", mExpectedTitle);
    }
}
