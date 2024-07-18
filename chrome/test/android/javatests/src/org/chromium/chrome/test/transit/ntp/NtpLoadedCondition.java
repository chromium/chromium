// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;

/** Fulfilled when a native New Tab Page is loaded. */
public class NtpLoadedCondition extends UiThreadCondition {
    private final Supplier<Tab> mLoadedTabSupplier;

    public NtpLoadedCondition(Supplier<Tab> loadedTabSupplier) {
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    public String buildDescription() {
        return "Ntp loaded";
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        Tab tab = mLoadedTabSupplier.get();

        NativePage nativePage = tab.getNativePage();
        if (nativePage == null) {
            return notFulfilled("tab.getNativePage() is null");
        }

        if (!tab.isIncognito()) {
            if (!(nativePage instanceof NewTabPage)) {
                return notFulfilled(
                        "native page has [type %s, title \"%s\"], waiting to be NewTabPage",
                        nativePage.getClass().getName(), nativePage.getTitle());
            }
            boolean isLoaded = ((NewTabPage) nativePage).isLoadedForTests();
            return whether(isLoaded, "native page is of type NewTabPage, isLoaded=%b", isLoaded);
        } else {
            if (!(nativePage instanceof IncognitoNewTabPage)) {
                return notFulfilled(
                        "native page has type [type %s, title \"%s\"], waiting to be"
                                + " IncognitoNewTabPage",
                        nativePage.getClass().getName(), nativePage.getTitle());
            }
            boolean isLoaded = ((IncognitoNewTabPage) nativePage).isLoadedForTests();
            return whether(
                    isLoaded, "native page is of type IncognitoNewTabPage, isLoaded=%b", isLoaded);
        }
    }
}
