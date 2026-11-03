// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static org.chromium.base.test.transit.Triggers.noopTo;

import android.app.Activity;

import org.junit.rules.RuleChain;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.ArrayList;
import java.util.List;

/**
 * Rule for integration tests that reuse a ChromeTabbedActivity but reset tab state between cases.
 *
 * <p>Tests using this should be batched.
 */
@NullMarked
public class AutoResetCtaTransitTestRule extends BaseCtaTransitTestRule {
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
                        .around(this::finishLeftoverActivitiesAfterTest)
                        .around(mBatchedRule);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return mChain.apply(statement, description);
    }

    /**
     * Finishes non-ChromeTabbedActivity Activities the test left open (e.g. after failing midway),
     * so they don't break later tests in the batch. Runs after the test so failures are attributed
     * to it. Already-finishing Activities are not waited on (~100ms/test).
     */
    private Statement finishLeftoverActivitiesAfterTest(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                Throwable failure = null;
                try {
                    base.evaluate();
                } catch (Throwable t) {
                    failure = t;
                }
                try {
                    finishNonTabbedActivities();
                } catch (Throwable t) {
                    if (failure == null) {
                        failure = t;
                    } else {
                        failure.addSuppressed(t);
                    }
                }
                if (failure != null) throw failure;
            }
        };
    }

    private static void finishNonTabbedActivities() {
        List<Activity> leakedActivities =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            List<Activity> result = new ArrayList<>();
                            for (Activity activity : ApplicationStatus.getRunningActivities()) {
                                if (activity instanceof ChromeTabbedActivity) continue;
                                if (!activity.isFinishing()) result.add(activity);
                            }
                            return result;
                        });
        if (leakedActivities.isEmpty()) return;

        Log.i(TAG, "Finishing Activities left open by the test: %s", leakedActivities);
        finishActivitiesWithCleanup(leakedActivities);
    }

    /**
     * Start the batched test in a blank page.
     *
     * <p>From the second test onwards, tab state was reset by {@link BlankCTATabInitialStateRule}.
     * Extra windows are closed here.
     */
    @Override
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
                finishActivityWithCleanup(station.getActivity());
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
    @Override
    public RegularNewTabPageStation startOnNtp() {
        WebPageStation blankPage = startOnBlankPage();
        return blankPage.loadNtp();
    }

    /**
     * Start the batched test at |url| arriving at the Station built by |builder|.
     *
     * <p>From the second test onwards, state was reset by {@link BlankCTATabInitialStateRule}.
     */
    @Override
    public <T extends CtaPageStation> T startOnPage(
            String url, BasePageStation.Builder<T> builder) {
        WebPageStation blankPage = startOnBlankPage();
        return blankPage.loadPageProgrammatically(url, builder);
    }
}
