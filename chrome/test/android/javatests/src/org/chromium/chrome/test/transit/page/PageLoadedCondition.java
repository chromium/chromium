// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.function.Supplier;

/** Fulfilled when a page is loaded. */
public class PageLoadedCondition extends ConditionWithResult<Tab> {
    private final Supplier<Tab> mActivityTabSupplier;
    private final @TriState int mExpectedIncognito;

    /**
     * @param activityTabSupplier Supplies the Tab to check.
     * @param expectedIncognito The incognito state the Tab must have, or {@link TriState#NOT_SET}
     *     to not check it.
     */
    public PageLoadedCondition(Supplier<Tab> activityTabSupplier, @TriState int expectedIncognito) {
        super(/* isRunOnUiThread= */ true);
        mActivityTabSupplier = dependOnSupplier(activityTabSupplier, "ActivityTab");
        mExpectedIncognito = expectedIncognito;
    }

    @Override
    public String buildDescription() {
        return switch (mExpectedIncognito) {
            case TriState.TRUE -> "Incognito tab loaded";
            case TriState.FALSE -> "Regular tab loaded";
            default -> "Tab loaded";
        };
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
                        "incognito %b, isLoading %b, hasWebContents %b, shouldShowLoadingUi %b",
                        isIncognito, isLoading, webContents != null, shouldShowLoadingUi);
        boolean incognitoMatches =
                mExpectedIncognito == TriState.NOT_SET
                        || TriStateUtils.from(isIncognito) == mExpectedIncognito;
        if (incognitoMatches && !isLoading && webContents != null && !shouldShowLoadingUi) {
            return fulfilled(message).withResult(tab);
        } else {
            return notFulfilled(message).withoutResult();
        }
    }
}
