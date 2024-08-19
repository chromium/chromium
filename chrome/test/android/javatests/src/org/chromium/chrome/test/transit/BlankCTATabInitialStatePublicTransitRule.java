// Copyright 2024 The Chromium Authors
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
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Wraps BlankCTATabInitialStateRule to be used in Public Transit batched tests. */
public class BlankCTATabInitialStatePublicTransitRule implements TestRule {

    private final ChromeTabbedActivityTestRule mActivityTestRule;

    public final BatchedPublicTransitRule<PageStation> mBatchedRule;

    public final BlankCTATabInitialStateRule mInitialStateRule;
    private final RuleChain mChain;

    public BlankCTATabInitialStatePublicTransitRule(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
        mBatchedRule =
                new BatchedPublicTransitRule<>(PageStation.class, /* expectResetByTest= */ false);
        mInitialStateRule = new BlankCTATabInitialStateRule(mActivityTestRule, true);
        mChain = RuleChain.outerRule(mBatchedRule).around(mInitialStateRule);
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
        Station homeStation = mBatchedRule.getHomeStation();
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
