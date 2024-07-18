// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** Fulfilled when a page is loaded. */
public class PageLoadedCondition extends ConditionWithResult<Tab> {
    private final Supplier<Tab> mActivityTabSupplier;
    private final boolean mIncognito;

    PageLoadedCondition(Supplier<Tab> activityTabSupplier, boolean incognito) {
        super(/* isRunOnUiThread= */ true);
        mActivityTabSupplier = dependOnSupplier(activityTabSupplier, "ActivityTab");
        mIncognito = incognito;
    }

    @Override
    public String buildDescription() {
        return mIncognito ? "Incognito tab loaded" : "Regular tab loaded";
    }

    @Override
    protected ConditionStatusWithResult<Tab> resolveWithSuppliers() {
        Tab tab = mActivityTabSupplier.get();

        boolean isIncognito = tab.isIncognito();
        boolean isLoading = tab.isLoading();
        WebContents webContents = tab.getWebContents();
        boolean shouldShowLoadingUi = webContents != null && webContents.shouldShowLoadingUI();
        String message =
                String.format(
                        "incognito %b, isLoading %b, hasWebContents %b, shouldShowLoadingUI %b",
                        isIncognito, isLoading, webContents != null, shouldShowLoadingUi);
        if (isIncognito == mIncognito
                && !isLoading
                && webContents != null
                && !shouldShowLoadingUi) {
            return fulfilled(message).withResult(tab);
        } else {
            return notFulfilled(message).withoutResult();
        }
    }
}
