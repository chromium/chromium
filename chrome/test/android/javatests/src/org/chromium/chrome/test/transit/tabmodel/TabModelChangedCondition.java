// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Condition fulfilled when tab model changes from regular to incognito or vice versa. */
public class TabModelChangedCondition extends InstrumentationThreadCondition {
    private final TabModelSelector mTabModelSelector;
    private Boolean mStartingModelIsIncognito;

    public TabModelChangedCondition(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        if (mStartingModelIsIncognito == null) return notFulfilled();
        boolean currentModelIsIncognito = mTabModelSelector.getCurrentModel().isIncognitoBranded();
        String message =
                "Expected tab model to switch. Starting Tab model is incognito: "
                        + mStartingModelIsIncognito
                        + ", Current Tab model is incognito: "
                        + currentModelIsIncognito;
        return whether(currentModelIsIncognito != mStartingModelIsIncognito, message);
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        mStartingModelIsIncognito = mTabModelSelector.getCurrentModel().isIncognitoBranded();
    }

    @Override
    public String buildDescription() {
        return "Tab model to switch with starting incognito state: " + mStartingModelIsIncognito;
    }
}
