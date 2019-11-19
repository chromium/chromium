// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.PowerManager;
import android.util.Pair;

import org.junit.Assert;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.omaha.OmahaBase;
import org.chromium.chrome.browser.omaha.VersionNumberGetter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Methods used for testing Chrome at the Application-level.
 */
public class ApplicationTestUtils {
    private static final String TAG = "ApplicationTestUtils";
    private static final float FLOAT_EPSILON = 0.001f;

    private static PowerManager.WakeLock sWakeLock;

    // TODO(jbudorick): fix deprecation warning crbug.com/537347
    @SuppressWarnings("deprecation")
    @SuppressLint("WakelockTimeout")
    public static void setUp(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            finishAllChromeTasks(context);
        }

        // Make sure the screen is on during test runs.
        PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        sWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK
                        | PowerManager.ACQUIRE_CAUSES_WAKEUP | PowerManager.ON_AFTER_RELEASE,
                "Chromium:" + TAG);
        sWakeLock.acquire();

        // Disable Omaha related activities.
        OmahaBase.setIsDisabledForTesting(true);
        VersionNumberGetter.setEnableUpdateDetection(false);
    }

    public static void tearDown(Context context) {
        Assert.assertNotNull("Uninitialized wake lock", sWakeLock);
        sWakeLock.release();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            try {
                finishAllChromeTasks(context);
            } catch (AssertionError exception) {
            }
        }
    }

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

    /** Waits until Chrome is in the background. */
    public static void waitUntilChromeInBackground() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int state = ApplicationStatus.getStateForApplication();
                boolean retVal = state == ApplicationState.HAS_STOPPED_ACTIVITIES
                        || state == ApplicationState.HAS_DESTROYED_ACTIVITIES;
                if (!retVal) updateVisibleActivitiesError();
                return retVal;
            }

            private void updateVisibleActivitiesError() {
                List<Pair<Activity, Integer>> visibleActivities = new ArrayList<>();
                for (Activity activity : ApplicationStatus.getRunningActivities()) {
                    @ActivityState
                    int activityState = ApplicationStatus.getStateForActivity(activity);
                    if (activityState != ActivityState.DESTROYED
                            && activityState != ActivityState.STOPPED) {
                        visibleActivities.add(Pair.create(activity, activityState));
                    }
                }
                if (visibleActivities.isEmpty()) {
                    updateFailureReason(
                            "No visible activities, application status response is suspect.");
                } else {
                    StringBuilder error = new StringBuilder("Unexpected visible activities: ");
                    for (Pair<Activity, Integer> visibleActivityState : visibleActivities) {
                        Activity activity = visibleActivityState.first;
                        error.append(
                                String.format(Locale.US, "\n\tActivity: %s, State: %d, Intent: %s",
                                        activity.getClass().getSimpleName(),
                                        visibleActivityState.second, activity.getIntent()));
                    }
                    updateFailureReason(error.toString());
                }
            }
        });
    }

    /** Waits until Chrome is in the foreground. */
    public static void waitUntilChromeInForeground() {
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(ApplicationState.HAS_RUNNING_ACTIVITIES, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return ApplicationStatus.getStateForApplication();
                    }
                }));
    }

    /** Finishes the given activity and waits for its onDestroy() to be called. */
    public static void finishActivity(final Activity activity) throws Exception {
        final CallbackHelper callbackHelper = new CallbackHelper();
        final ApplicationStatus.ActivityStateListener activityStateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        if (newState == ActivityState.DESTROYED) {
                            callbackHelper.notifyCalled();
                        }
                    }
                };
        try {
            boolean alreadyDestroyed =
                    TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                        @Override
                        public Boolean call() {
                            if (ApplicationStatus.getStateForActivity(activity)
                                    == ActivityState.DESTROYED) {
                                return true;
                            }
                            ApplicationStatus.registerStateListenerForActivity(
                                    activityStateListener, activity);
                            activity.finish();
                            return false;
                        }
                    });
            if (!alreadyDestroyed) {
                callbackHelper.waitForCallback(0);
            }
        } finally {
            ApplicationStatus.unregisterActivityStateListener(activityStateListener);
        }
    }

    /** Finishes all tasks Chrome has listed in Android's Overview. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static void finishAllChromeTasks(final Context context) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                // Close all of the tasks one by one.
                ActivityManager activityManager =
                        (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
                for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                    task.finishAndRemoveTask();
                }
            } catch (Exception e) {
                // Ignore any exceptions the Android framework throws so that otherwise passing
                // tests don't fail during tear down. See crbug.com/653731.
            }
        });

        CriteriaHelper.pollUiThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return getNumChromeTasks(context);
            }
        }));
    }

    /** Counts how many tasks Chrome has listed in Android's Overview. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static int getNumChromeTasks(Context context) {
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        return activityManager.getAppTasks().size();
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
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab tab = activity.getActivityTab();
                if (tab == null) return false;

                Coordinates coord = Coordinates.createFor(tab.getWebContents());
                float scale = coord.getPageScaleFactor();
                updateFailureReason(
                        "Expecting scale factor of: " + expectedScale + ", got: " + scale);
                return Math.abs(scale - expectedScale) < FLOAT_EPSILON;
            }
        });
    }

    /**
     * Recreates the provided Activity, returning the newly created Activity once it's finished
     * starting up.
     * @param activity The Activity to recreate.
     * @return The newly created Activity.
     */
    public static <T extends Activity> T recreateActivity(T activity) {
        final Class<?> activityClass = activity.getClass();
        final CallbackHelper activityCallback = new CallbackHelper();
        final AtomicReference<T> activityRef = new AtomicReference<>();
        ApplicationStatus.ActivityStateListener stateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @SuppressWarnings("unchecked")
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        if (newState == ActivityState.RESUMED) {
                            if (!activityClass.isAssignableFrom(activity.getClass())) return;

                            activityRef.set((T) activity);
                            new Handler().post(() -> activityCallback.notifyCalled());
                            ApplicationStatus.unregisterActivityStateListener(this);
                        }
                    }
                };
        ApplicationStatus.registerStateListenerForAllActivities(stateListener);

        try {
            TestThreadUtils.runOnUiThreadBlocking(() -> activity.recreate());
            activityCallback.waitForCallback("Activity did not start as expected", 0);
            T createdActivity = activityRef.get();
            Assert.assertNotNull("Activity reference is null.", createdActivity);
            return createdActivity;
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        } finally {
            ApplicationStatus.unregisterActivityStateListener(stateListener);
        }
    }
}
