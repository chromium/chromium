// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;

/** Fulfilled when a page is interactable (or hidden). */
class PageInteractableCondition extends UiThreadCondition {
    private final PageLoadedCondition mPageLoadedCondition;

    PageInteractableCondition(PageLoadedCondition pageLoadedCondition) {
        mPageLoadedCondition = pageLoadedCondition;
    }

    @Override
    public String buildDescription() {
        return "Page interactable or hidden";
    }

    @Override
    public boolean check() {
        Tab tab = mPageLoadedCondition.getMatchedTab();
        if (tab == null) {
            return false;
        }

        return tab.isUserInteractable() || tab.isHidden();
    }
}
