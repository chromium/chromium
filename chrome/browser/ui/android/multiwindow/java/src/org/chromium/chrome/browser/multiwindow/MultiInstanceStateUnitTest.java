// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.ComponentName;
import android.text.TextUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for MultiInstanceState. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MultiInstanceStateUnitTest.ShadowAndroidTaskUtils.class})
public class MultiInstanceStateUnitTest {
    @Implements(AndroidTaskUtils.class)
    static class ShadowAndroidTaskUtils {
        @Implementation
        public static RecentTaskInfo getTaskInfoFromTask(AppTask task) {
            return sTasks.get(task);
        }
    }

    private static Map<AppTask, RecentTaskInfo> sTasks = new HashMap<>();

    private MultiInstanceState mMultiInstanceState;

    private String mBaseActivityClassName = BrowserActivity.class.getName();

    private static class BaseActivity extends Activity {
        private int mTaskId;

        private void setTaskId(int taskId) {
            mTaskId = taskId;
        }

        @Override
        public int getTaskId() {
            return mTaskId;
        }

        public void stop() {
            ApplicationStatus.onStateChangeForTesting(this, ActivityState.STOPPED);
        }

        @Override
        public void destroy() {
            AppTask appTask = null;
            for (AppTask task : sTasks.keySet()) {
                if (AndroidTaskUtils.getTaskInfoFromTask(task).id == mTaskId) appTask = task;
            }
            sTasks.remove(appTask);
            ApplicationStatus.onStateChangeForTesting(this, ActivityState.DESTROYED);
        }
    }

    // Base activity for browser app.
    private static class BrowserActivity extends BaseActivity {}

    // Base activity for custom tab.
    private static class CustomTabActivity extends BaseActivity {}

    private static class ObserverHelper extends CallbackHelper {
        private int mCount;

        private void assertObserverCalled(String msg) {
            assertObserver(true, "Observer was not called: " + msg);
        }

        private void assertObserverNotCalled(String msg) {
            assertObserver(false, "Observer should not have been called: " + msg);
        }

        private void assertObserver(boolean called, String message) {
            try {
                if (called) {
                    Assert.assertTrue(message, getCallCount() == mCount + 1);
                } else {
                    Assert.assertFalse(message, getCallCount() == mCount + 1);
                }
            } finally {
                mCount = getCallCount();
            }
        }
    }

    @Before
    public void setUp() {
        MultiInstanceState.maybeCreate(this::getChromeTasks, this::matchesBaseActivity);
        mMultiInstanceState = MultiInstanceState.getInstanceForTesting();
    }

    @After
    public void tearDown() {
        ApplicationStatus.destroyForJUnitTests();
        sTasks.clear();
        mMultiInstanceState.clear();
        mMultiInstanceState = null;
    }

    private BaseActivity createTaskAndLaunchActivity(int taskId, BaseActivity activity) {
        activity.setTaskId(taskId);
        RecentTaskInfo taskInfo = new RecentTaskInfo();
        taskInfo.id = taskId;
        taskInfo.baseActivity =
                new ComponentName(
                        activity.getClass().getPackage().getName(), activity.getClass().getName());
        sTasks.put(new AppTask(null), taskInfo);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        return activity;
    }

    private List<AppTask> getChromeTasks() {
        return new ArrayList<AppTask>(sTasks.keySet());
    }

    private boolean matchesBaseActivity(String name) {
        return TextUtils.equals(name, mBaseActivityClassName);
    }

    @Test
    public void testVisibilityObserverForMultiInstance() {
        ObserverHelper helper = new ObserverHelper();
        mMultiInstanceState.addObserver((visible) -> helper.notifyCalled());

        BaseActivity baseActivity1 = createTaskAndLaunchActivity(29, new BrowserActivity());
        assertInSingleInstanceMode("initial state");

        BaseActivity baseActivity2 = createTaskAndLaunchActivity(31, new BrowserActivity());
        helper.assertObserverCalled("a resume event for an activity of the 2nd task triggered");
        assertInMultiInstanceMode("the new task's base activity was resumed");

        BaseActivity baseActivity3 = createTaskAndLaunchActivity(37, new BrowserActivity());
        helper.assertObserverNotCalled("already in multi-instance mode");
        assertInMultiInstanceMode("already in multi-instance mode");

        baseActivity3.stop();
        helper.assertObserverNotCalled("already in multi-instance mode");
        assertInMultiInstanceMode("already in multi-instance mode");

        baseActivity2.stop();
        // Only task29 (BaseActivity1) is visible now. Multi-instance state goes off.
        helper.assertObserverCalled("A single visible task left");
        assertInSingleInstanceMode("A single visible task left");
    }

    @Test
    public void testRuleOutOtherBaseActivityTasks() {
        BaseActivity baseActivity1 = createTaskAndLaunchActivity(29, new CustomTabActivity());
        BaseActivity baseActivity2 = createTaskAndLaunchActivity(31, new CustomTabActivity());
        assertInSingleInstanceMode("Base activity is not legit: " + baseActivity1);
    }

    private void assertInMultiInstanceMode(String msg) {
        Assert.assertTrue(
                "Should be in multi-instance mode: " + msg,
                mMultiInstanceState.isInMultiInstanceMode());
    }

    private void assertInSingleInstanceMode(String msg) {
        Assert.assertFalse(
                "Should be in single-instance mode: " + msg,
                mMultiInstanceState.isInMultiInstanceMode());
    }
}
