// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.chromium.base.test.transit.Condition.whether;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.tab.Tab;

import java.util.function.Supplier;

/** Condition to check the page url contains a certain substring. */
public class PageUrlContainsCondition extends Condition {
    private final String mExpectedUrlPiece;
    private final Supplier<Tab> mLoadedTabSupplier;

    public PageUrlContainsCondition(String expectedUrl, Supplier<Tab> loadedTabSupplier) {
        super(/* isRunOnUiThread= */ true);
        mExpectedUrlPiece = expectedUrl;
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        String url = mLoadedTabSupplier.get().getUrl().getSpec();
        return whether(url.contains(mExpectedUrlPiece), "ActivityTab url: \"%s\"", url);
    }

    @Override
    public String buildDescription() {
        return String.format("URL of activity tab contains \"%s\"", mExpectedUrlPiece);
    }
}
