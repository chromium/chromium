// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;

/** Fulfilled when a native New Tab Page is loaded. */
class NtpLoadedCondition extends UiThreadCondition {
    private final PageLoadedCondition mPageLoadedCondition;

    NtpLoadedCondition(PageLoadedCondition pageLoadedCondition) {
        super();
        mPageLoadedCondition = pageLoadedCondition;
    }

    @Override
    public String buildDescription() {
        return "Ntp loaded";
    }

    @Override
    public boolean check() {
        Tab tab = mPageLoadedCondition.getMatchedTab();
        if (tab == null) {
            return false;
        }

        NativePage nativePage = tab.getNativePage();
        if (!tab.isIncognito()) {
            return (nativePage instanceof NewTabPage)
                    && ((NewTabPage) nativePage).isLoadedForTests();
        } else {
            return (nativePage instanceof IncognitoNewTabPage)
                    && ((IncognitoNewTabPage) nativePage).isLoadedForTests();
        }
    }
}
