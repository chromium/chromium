// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.Elements;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * The New Tab Page screen, with an omnibox, most visited tiles, and the Feed instead of the
 * WebContents.
 */
public class NewTabPageStation extends PageStation {
    public NewTabPageStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule,
            boolean incognito,
            boolean isOpeningTab) {
        super(chromeTabbedActivityTestRule, incognito, isOpeningTab);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedEnterCondition));
    }
}
