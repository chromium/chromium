// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.util.Pair;

import org.hamcrest.Matchers;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.Coordinates;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Methods used for testing Chrome at the Application-level. */
public class ChromeApplicationTestUtils {
    private static final String TAG = "ApplicationTestUtils";
    private static final float FLOAT_EPSILON = 0.001f;

    // Increase the default timeout, as it can take a long time for Android to
    // fully stop/start Chrome.
    private static final long CHROME_STOP_START_TIMEOUT_MS =
            Math.max(10000L, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);

    // TODO(bauerb): make this function throw more specific exception and update
    // StartupLoadingMetricsTest correspondingly.
    /** Send the user to the Android home screen. */
    public static void fireHomeScreenIntent(Context context) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        waitUntilChromeInBackground();
    }

    /** Simulate starting Chrome from the launcher with a Main Intent. */
    public static void launchChrome(Context context) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(context.getPackageName());
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        waitUntilChromeInForeground();
    }

    private static String getVisibleActivitiesError() {
        List<Pair<Activity, Integer>> visibleActivities = new ArrayList<>();
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            @ActivityState int activityState = ApplicationStatus.getStateForActivity(activity);
            if (activityState != ActivityState.DESTROYED
                    && activityState != ActivityState.STOPPED) {
                visibleActivities.add(Pair.create(activity, activityState));
            }
        }
        if (visibleActivities.isEmpty()) {
            return "No visible activities, application status response is suspect.";
        } else {
            StringBuilder error = new StringBuilder("Unexpected visible activities: ");
            for (Pair<Activity, Integer> visibleActivityState : visibleActivities) {
                Activity activity = visibleActivityState.first;
                error.append(
                        String.format(
                                Locale.US,
                                "\n\tActivity: %s, State: %d, Intent: %s",
                                activity.getClass().getSimpleName(),
                                visibleActivityState.second,
                                activity.getIntent()));
            }
            return error.toString();
        }
    }

    /** Waits until Chrome is in the background. */
    public static void waitUntilChromeInBackground() {
        CriteriaHelper.pollUiThread(
                () -> {
                    int state = ApplicationStatus.getStateForApplication();
                    Criteria.checkThat(
                            getVisibleActivitiesError(),
                            state,
                            Matchers.anyOf(
                                    Matchers.equalTo(ApplicationState.HAS_STOPPED_ACTIVITIES),
                                    Matchers.equalTo(ApplicationState.HAS_DESTROYED_ACTIVITIES)));
                },
                CHROME_STOP_START_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Waits until Chrome is in the foreground. */
    public static void waitUntilChromeInForeground() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getStateForApplication(),
                            Matchers.is(ApplicationState.HAS_RUNNING_ACTIVITIES));
                },
                CHROME_STOP_START_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits till the WebContents receives the expected page scale factor
     * from the compositor and asserts that this happens.
     *
     * Proper use of this function requires waiting for a page scale factor that isn't 1.0f because
     * the default seems to be 1.0f.
     */
    public static void assertWaitForPageScaleFactorMatch(
            final ChromeActivity activity, final float expectedScale) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = activity.getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());

                    Coordinates coord = Coordinates.createFor(tab.getWebContents());
                    float scale = coord.getPageScaleFactor();
                    Criteria.checkThat(
                            (double) scale,
                            Matchers.is(Matchers.closeTo(expectedScale, FLOAT_EPSILON)));
                });
    }
}
