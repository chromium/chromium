// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** Fulfilled when WebContents are present in the Tab supplied. */
public class WebContentsPresentCondition extends ConditionWithResult<WebContents> {
    private final Supplier<Tab> mLoadedTabSupplier;

    public WebContentsPresentCondition(Supplier<Tab> loadedTabSupplier) {
        super(/* isRunOnUiThread= */ false);
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    protected ConditionStatusWithResult<WebContents> resolveWithSuppliers() {
        return whether(hasValue()).withResult(get());
    }

    @Override
    public String buildDescription() {
        return "WebContents present";
    }

    @Override
    public WebContents get() {
        // Do not return a WebContents that has been destroyed, so always get it from the
        // Tab instead of letting ConditionWithResult return its |mResult|.
        if (!mLoadedTabSupplier.hasValue()) {
            return null;
        }
        Tab loadedTab = mLoadedTabSupplier.get();
        if (loadedTab == null) {
            return null;
        }
        return loadedTab.getWebContents();
    }

    @Override
    public boolean hasValue() {
        return get() != null;
    }
}
