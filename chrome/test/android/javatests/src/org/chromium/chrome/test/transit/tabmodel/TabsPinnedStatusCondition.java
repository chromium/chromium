// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;
import java.util.Locale;

/** Condition fulfilled when the pinned status of the provided tabs is as expected. */
public class TabsPinnedStatusCondition extends UiThreadCondition {
    private final TabModel mTabModel;
    private final List<@TabId Integer> mTabIdsToCheck;
    private final boolean mShouldBePinned;

    public TabsPinnedStatusCondition(
            TabModel tabModel, List<@TabId Integer> tabIdsToCheck, boolean shouldBePinned) {
        mTabModel = tabModel;
        mTabIdsToCheck = tabIdsToCheck;
        mShouldBePinned = shouldBePinned;
    }

    @Override
    public ConditionStatus checkWithSuppliers() {
        for (@TabId Integer tabId : mTabIdsToCheck) {
            Tab tab = mTabModel.getTabById(tabId);

            if (tab == null) {
                return notFulfilled("Tab %s not found", tabId);
            }

            boolean isTabPinned = tab.getIsPinned();
            if (isTabPinned != mShouldBePinned) {
                return notFulfilled(
                        "Tab %s has isPinned=%b, expected isPinned=%b",
                        tabId, isTabPinned, mShouldBePinned);
            }
        }
        return fulfilled("All tabs match expected pinned state");
    }

    @Override
    public String buildDescription() {
        return String.format(
                Locale.ROOT,
                "%d tabs should all be %s",
                mTabIdsToCheck.size(),
                mShouldBePinned ? "pinned" : "unpinned");
    }
}
