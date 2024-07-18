// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Condition fulfilled when N number of tabs are opened/closed. */
public class TabCountChangedCondition extends InstrumentationThreadCondition {
    private TabModel mTabModel;
    private int mStartingTabs = -1;
    private final int mExpectedChange;

    public TabCountChangedCondition(TabModel tabModel, int expectedChange) {
        mTabModel = tabModel;
        mExpectedChange = expectedChange;
    }

    @Override
    public ConditionStatus checkWithSuppliers() {
        // If mStartingTabs == -1 means onStartMonitoring has yet to be called
        // so we need to wait.
        if (mStartingTabs == -1) {
            return notFulfilled();
        }
        int expectedTabCount = mStartingTabs + mExpectedChange;
        int currentTabCount = mTabModel.getCount();
        String message =
                "Starting Tabs: "
                        + mStartingTabs
                        + ", Expected Tabs: "
                        + expectedTabCount
                        + ", Current Tabs: "
                        + currentTabCount;
        return whether(currentTabCount == expectedTabCount, message);
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        mStartingTabs = mTabModel.getCount();
    }

    @Override
    public String buildDescription() {
        String tabType = mTabModel.isOffTheRecord() ? "Incognito" : "Regular";
        return tabType + " Tab count changed by " + mExpectedChange;
    }
}
