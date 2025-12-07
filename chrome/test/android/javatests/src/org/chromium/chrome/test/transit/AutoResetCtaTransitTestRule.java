// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static org.chromium.base.test.transit.Triggers.noopTo;

import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.List;

/**
 * Rule for integration tests that reuse a ChromeTabbedActivity but reset tab state between cases.
 *
 * <p>Tests using this should be batched.
 */
@NullMarked
public class AutoResetCtaTransitTestRule extends BaseCtaTransitTestRule implements TestRule {
    private static final String TAG = "Transit";
    private final BlankCTATabInitialStateRule mInitialStateRule;
    private final BatchedPublicTransitRule<CtaPageStation> mBatchedRule;
    private final RuleChain mChain;

    /** Create with {@link ChromeTransitTestRules#autoResetCtaActivityRule()}. */
    AutoResetCtaTransitTestRule(boolean clearAllTabState) {
        super();
        mBatchedRule =
                new BatchedPublicTransitRule<>(
                        CtaPageStation.class, /* expectResetByTest= */ false);
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
     * <p>From the second test onwards, tab state was reset by {@link BlankCTATabInitialStateRule}.
     * Extra windows are closed here.
     */
    public WebPageStation startOnBlankPage() {
        // Empty in the first test, should be size 1+ from the second test onwards.
        // When we encounter size 2+, close the extra windows left by the previous test.
        List<Station<?>> activeStations = TrafficControl.getActiveStations();
        int numWindows = activeStations.size();
        if (numWindows > 1) {
            Log.i(TAG, "%d active Stations, close all windows but the first", numWindows);
            Log.i(TAG, "Keep first window at %s", activeStations.get(0));
            for (Station<?> station : activeStations.subList(1, numWindows)) {
                Log.i(TAG, "Close window at %s", station);
                station.getActivity().finish();
            }
        }

        // Remove the last station of the previous test from |activeStations| to go to an entry
        // point again.
        TrafficControl.hopOffPublicTransit();

        WebPageStation entryPageStation = WebPageStation.newBuilder().withEntryPoint().build();

        // Wait for the Conditions to be met to return an active PageStation.
        return noopTo().inNewTask().arriveAt(entryPageStation);
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

    /**
     * Start the batched test in a URL.
     *
     * <p>From the second test onwards, state was reset by {@link BlankCTATabInitialStateRule}.
     */
    public WebPageStation startOnWebPage(String url) {
        WebPageStation blankPage = startOnBlankPage();
        return blankPage.loadWebPageProgrammatically(url);
    }
}
