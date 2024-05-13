// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

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
        WebPageStation entryPageStation =
                WebPageStation.newWebPageStationBuilder()
                        .withActivityTestRule(mActivityTestRule)
                        .withEntryPoint()
                        .build();

        // Null in the first test, non-null from the second test onwards.
        PageStation homeStation = mBatchedRule.getHomeStation();

        // Wait for the Conditions to be met to return an active PageStation.
        return Trip.travelSync(/* origin= */ homeStation, entryPageStation, /* trigger= */ null);
    }
}
