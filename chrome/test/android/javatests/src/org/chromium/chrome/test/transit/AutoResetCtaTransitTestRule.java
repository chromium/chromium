// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Rule for integration tests that reuse a ChromeTabbedActivity but reset tab state between cases.
 *
 * <p>Tests using this should be batched.
 */
@NullMarked
public class AutoResetCtaTransitTestRule extends BaseCtaTransitTestRule implements TestRule {
    private final BlankCTATabInitialStateRule mInitialStateRule;
    private final BatchedPublicTransitRule<PageStation> mBatchedRule;
    private final RuleChain mChain;

    /** Create with {@link ChromeTransitTestRules#autoResetCtaActivityRule()}. */
    AutoResetCtaTransitTestRule(boolean clearAllTabState) {
        super();
        mBatchedRule =
                new BatchedPublicTransitRule<>(PageStation.class, /* expectResetByTest= */ false);
        mInitialStateRule = new BlankCTATabInitialStateRule(mActivityTestRule, clearAllTabState);
        mChain =
                RuleChain.outerRule(mActivityTestRule)
                        .around(mInitialStateRule)
                        .around(mBatchedRule);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return mChain.apply(statement, description);
    }

    /**
     * Start the batched test in a blank page.
     *
     * <p>From the second test onwards, state was reset by {@link BlankCTATabInitialStateRule}.
     */
    public WebPageStation startOnBlankPage() {
        // Null in the first test, non-null from the second test onwards.
        Station<?> homeStation = TrafficControl.getActiveStation();
        if (homeStation == null) {
            EntryPointSentinelStation entryPoint = new EntryPointSentinelStation();
            entryPoint.setAsEntryPoint();
            homeStation = entryPoint;
        }

        WebPageStation entryPageStation = WebPageStation.newBuilder().withEntryPoint().build();

        // Wait for the Conditions to be met to return an active PageStation.
        return homeStation.travelToSync(entryPageStation, /* trigger= */ null);
    }

    /**
     * Start the batched test in an NTP.
     *
     * <p>From the second test onwards, state was reset by {@link BlankCTATabInitialStateRule}.
     */
    public RegularNewTabPageStation startOnNtp() {
        WebPageStation blankPage = startOnBlankPage();
        return blankPage.loadPageProgrammatically(
                UrlConstants.NTP_URL, RegularNewTabPageStation.newBuilder());
    }
}
