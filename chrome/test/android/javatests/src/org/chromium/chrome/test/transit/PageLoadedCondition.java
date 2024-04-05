// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.WebContents;

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
        return mIncognito ? "Incognito tab loaded" : "Regular tab loaded";
    }

    @Override
    public ConditionStatus check() {
        Tab tab = mChromeTabbedActivityTestRule.getActivity().getActivityTab();
        if (tab == null) {
            return notFulfilled("null ActivityTab");
        }

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
            mMatchedTab = tab;
            return fulfilled(message);
        } else {
            return notFulfilled(message);
        }
    }

    public Tab getMatchedTab() {
        return mMatchedTab;
    }
}
