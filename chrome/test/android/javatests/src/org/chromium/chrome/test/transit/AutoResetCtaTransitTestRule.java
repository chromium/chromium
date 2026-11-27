// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner.ClassCleanupHook;
import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.Outcome;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 * Rule for integration tests that reuse a ChromeTabbedActivity but reset tab state to a single
 * blank tab between cases.
 *
 * <p>Activity state is stored statically, so the Activity may be reused across multiple test suites
 * within the same @Batch. Tests using this should be batched.
 */
@NullMarked
public class AutoResetCtaTransitTestRule extends BaseCtaTransitTestRule {
    private static final String TAG = "Transit";
    // Kept to notify package-private TrafficControl.onTestStarted() / onTestFinished().
    private static final BatchedPublicTransitRule<CtaPageStation> sBatchedRule =
            new BatchedPublicTransitRule<>(CtaPageStation.class, /* expectResetByTest= */ false);
    private static @Nullable WeakReference<ChromeTabbedActivity> sActivity;

    private final boolean mClearAllTabState;
    private final ChromeNightModeState mNightMode = new ChromeNightModeState();

    /** Create with {@link ChromeTransitTestRules#autoResetCtaActivityRule()}. */
    AutoResetCtaTransitTestRule(boolean clearAllTabState) {
        super();
        mClearAllTabState = clearAllTabState;
        mActivityTestRule.setFinishActivity(false);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        // Nesting (outer -> inner): mActivityTestRule -> reset/cleanup -> sBatchedRule -> test.
        Statement batchedStatement = sBatchedRule.apply(statement, description);
        Statement resetStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        evaluateWithResetAndCleanup(batchedStatement);
                    }
                };
        return mActivityTestRule.apply(resetStatement, description);
    }

    /**
     * Requests a ChromeTabbedActivity in night mode (or light mode) for this test.
     *
     * <p>Call before startOn*(), e.g. from the test constructor (for @ClassParameter) or from
     * a @UseMethodParameterBefore method. The Activity is reused while the night mode stays the
     * same and relaunched only when it changes (at most twice per class with NightModeParams).
     *
     * <p>Tests that do not call this get the system default night mode.
     */
    public void setNightModeEnabled(boolean nightModeEnabled) {
        mNightMode.setRequested(nightModeEnabled);
    }

    private void evaluateWithResetAndCleanup(Statement batchedStatement) throws Throwable {
        ensureActivityInInitialState();
        Throwable failure = null;
        try {
            batchedStatement.evaluate();
        } catch (Throwable t) {
            failure = t;
        }
        try {
            finishNonTabbedActivities();
        } catch (Throwable t) {
            // Keep the test's own failure as the primary one.
            if (failure == null) {
                failure = t;
            } else {
                failure.addSuppressed(t);
            }
        } finally {
            // If the activity was relaunched during the test, update the reference to use the most
            // up to date Activity.
            setReusableActivity(mActivityTestRule.getActivity());
        }
        if (failure != null) throw failure;
    }

    @VisibleForTesting
    void ensureActivityInInitialState() {
        ChromeTabbedActivity cta = getReusableActivity();
        if (mNightMode.needsLaunchBeforeTest(cta)) {
            relaunchForNightMode();
        } else if (cta == null) {
            launchFreshActivity();
        } else {
            // Reuse the existing activity instance and reset tab state.
            mActivityTestRule.setActivity(cta);
            CtaStateResetter.resetToSingleBlankTab(mActivityTestRule, mClearAllTabState);
        }
    }

    private @Nullable ChromeTabbedActivity launchFreshActivity() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        setReusableActivity(cta);

        // Previous tests may have left tabs open and finished the Activity.
        if (tabsCount(/* incognito= */ false) > 1) {
            CtaStateResetter.resetTabStateFast(mActivityTestRule);
        }
        return cta;
    }

    /**
     * Finishes non-ChromeTabbedActivity Activities the test left open (e.g. after failing midway),
     * so they don't break later tests in the batch. Runs after the test so failures are attributed
     * to it. Already-finishing Activities are not waited on (~100ms/test).
     */
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

    private static @Nullable ChromeTabbedActivity getReusableActivity() {
        ChromeTabbedActivity cta = sActivity == null ? null : sActivity.get();
        if (cta == null || cta.isActivityFinishingOrDestroyed()) {
            sActivity = null;
            return null;
        }
        return cta;
    }

    private static void setReusableActivity(@Nullable ChromeTabbedActivity activity) {
        sActivity =
                activity == null || activity.isActivityFinishingOrDestroyed()
                        ? null
                        : new WeakReference<>(activity);
    }

    private void relaunchForNightMode() {
        finishAllActivities();
        mActivityTestRule.setActivity(null);
        mNightMode.launchWithAppliedPreference(this::launchFreshActivity);
    }

    /** Finishes all Activities (including already-finishing ones) so the next launch is fresh. */
    private static void finishAllActivities() {
        List<Activity> runningActivities =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new ArrayList<>(ApplicationStatus.getRunningActivities()));
        if (!runningActivities.isEmpty()) {
            TrafficControl.hopOffPublicTransit();
            finishActivitiesWithCleanup(runningActivities);
        }
        setReusableActivity(null);
    }

    /**
     * Start the batched test in a blank page.
     *
     * <p>From the second test onwards, tab state was reset before the test started. Extra windows
     * are closed here.
     */
    @Override
    public WebPageStation startOnBlankPage() {
        // TODO(crbug.com/543896771): Delete once AutoResetCtaTransitTestRule launches lazily.
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (mNightMode.needsRelaunch(
                cta != null && !cta.isActivityFinishingOrDestroyed() ? cta : null)) {
            relaunchForNightMode();
        }

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

        // No Trigger: wait for the Conditions to be met to return an active PageStation in the
        // existing window, which is a new task to Public Transit after hopping off.
        WebPageStation entryPageStation = WebPageStation.newBuilder().withEntryPoint().build();
        return Outcome.enter(entryPageStation).inNewTask().await();
    }

    /**
     * Start the batched test in an NTP.
     *
     * <p>From the second test onwards, state was reset before the test started.
     */
    @Override
    public RegularNewTabPageStation startOnNtp() {
        WebPageStation blankPage = startOnBlankPage();
        return blankPage.loadNtp();
    }

    /**
     * Start the batched test at |url| arriving at the Station built by |builder|.
     *
     * <p>From the second test onwards, state was reset before the test started.
     */
    @Override
    public <T extends CtaPageStation> T startOnPage(
            String url, BasePageStation.Builder<T> builder) {
        WebPageStation blankPage = startOnBlankPage();
        return blankPage.loadPageProgrammatically(url, builder);
    }

    @ServiceImpl(ClassCleanupHook.class)
    public static class CleanupHook implements ClassCleanupHook {
        @Override
        public void onAfterTestClass(Class<?> testClass) {
            // Don't let the next class in the batch reuse an Activity themed by this one.
            ChromeNightModeState.resetAfterTestClass(
                    AutoResetCtaTransitTestRule::finishAllActivities);
        }
    }
}
