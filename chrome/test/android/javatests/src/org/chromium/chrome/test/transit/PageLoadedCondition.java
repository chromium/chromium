// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Fulfilled when a page is loaded. */
class PageLoadedCondition extends UiThreadCondition {
    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    private final boolean mIncognito;
    private Tab mMatchedTab;

    PageLoadedCondition(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
        mIncognito = incognito;
    }

    @Override
    public String buildDescription() {
        return "Tab loaded";
    }

    @Override
    public boolean check() {
        Tab tab = mChromeTabbedActivityTestRule.getActivity().getActivityTab();
        if (tab != null
                && tab.isIncognito() == mIncognito
                && !tab.isLoading()
                && !tab.getWebContents().shouldShowLoadingUI()) {
            mMatchedTab = tab;
            return true;
        } else {
            return false;
        }
    }

    public Tab getMatchedTab() {
        return mMatchedTab;
    }
}
