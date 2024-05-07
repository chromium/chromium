// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;

/** Fulfilled when a page is interactable (or hidden). */
class PageInteractableOrHiddenCondition extends UiThreadCondition {
    private final Supplier<Tab> mLoadedTabSupplier;

    PageInteractableOrHiddenCondition(Supplier<Tab> loadedTabSupplier) {
        mLoadedTabSupplier = loadedTabSupplier;
    }

    @Override
    public String buildDescription() {
        return "Page interactable or hidden";
    }

    @Override
    public ConditionStatus check() {
        Tab tab = mLoadedTabSupplier.get();
        if (tab == null) {
            return notFulfilled("no loaded tab");
        }

        boolean isUserInteractable = tab.isUserInteractable();
        boolean isHidden = tab.isHidden();
        return whether(
                isUserInteractable || isHidden,
                "isUserInteractable=%b, isHidden=%b",
                isUserInteractable,
                isHidden);
    }
}
