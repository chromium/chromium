// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import android.app.Activity;
import android.os.Build;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.ui.messages.test.R;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link SnackbarManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SnackbarTest {
    private SnackbarManager mManager;
    private SnackbarController mDefaultController = new SnackbarController() {
        @Override
        public void onDismissNoAction(Object actionData) {}

        @Override
        public void onAction(Object actionData) {}
    };

    private SnackbarController mDismissController = new SnackbarController() {
        @Override
        public void onDismissNoAction(Object actionData) {
            mDismissed = true;
        }

        @Override
        public void onAction(Object actionData) {}
    };

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static FrameLayout sMainParent;
    private static FrameLayout sAlternateParent;
    private boolean mDismissed;

    @BeforeClass
    public static void setupSuite() {
        BlankUiTestActivity.setTestLayout(R.layout.test_snackbar_manager_activity_layout);
        activityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivity = activityTestRule.getActivity();
            sMainParent = sActivity.findViewById(android.R.id.content);
            sAlternateParent = sActivity.findViewById(R.id.alternate_parent);
            SnackbarManager.setDurationForTesting(1000);
        });
    }

    @AfterClass
    public static void teardownSuite() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SnackbarManager.restDurationForTesting();
            sActivity = null;
            sMainParent = null;
            sAlternateParent = null;
        });
    }

    @Before
    public void setupTest() {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { mManager = new SnackbarManager(sActivity, sMainParent, null); });
    }

    @Test
    @MediumTest
    public void testStackQueuePersistentOrder() {
        final Snackbar stackbar = Snackbar.make(
                "stack", mDefaultController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar queuebar = Snackbar.make("queue", mDefaultController,
                Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar persistent = Snackbar.make("persistent", mDefaultController,
                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(stackbar));
        pollSnackbarCondition("First snackbar not shown",
                () -> mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == stackbar);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.showSnackbar(queuebar);
            Assert.assertTrue("Snackbar not showing", mManager.isShowing());
            Assert.assertEquals("Snackbars on stack should not be cancelled by snackbars on queue",
                    stackbar, mManager.getCurrentSnackbarForTesting());
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.showSnackbar(persistent);
            Assert.assertTrue("Snackbar not showing", mManager.isShowing());
            Assert.assertEquals(
                    "Snackbars on stack should not be cancelled by persistent snackbars", stackbar,
                    mManager.getCurrentSnackbarForTesting());
        });
        pollSnackbarCondition("Snackbar on queue not shown",
                () -> mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == queuebar);
        pollSnackbarCondition("Snackbar on queue not timed out",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarForTesting() == persistent);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.onClick(null));
        pollSnackbarCondition(
                "Persistent snackbar did not get cleared", () -> !mManager.isShowing());
    }

    @Test
    @SmallTest
    public void testPersistentQueueStackOrder() {
        final Snackbar stackbar = Snackbar.make(
                "stack", mDefaultController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar queuebar = Snackbar.make("queue", mDefaultController,
                Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar persistent = Snackbar.make("persistent", mDefaultController,
                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(persistent));
        pollSnackbarCondition("First snackbar not shown",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarForTesting() == persistent);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(queuebar));
        pollSnackbarCondition("Persistent snackbar was not cleared by queue snackbar",
                () -> mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == queuebar);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(stackbar));
        pollSnackbarCondition("Snackbar on queue was not cleared by snackbar stack.",
                () -> mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == stackbar);
        pollSnackbarCondition("Snackbar did not time out",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarForTesting() == persistent);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.onClick(null));
        pollSnackbarCondition(
                "Persistent snackbar did not get cleared", () -> !mManager.isShowing());
    }

    @Test
    @SmallTest
    public void testDismissSnackbar() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        mDismissed = false;
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(snackbar));
        pollSnackbarCondition("Snackbar on queue was not cleared by snackbar stack.",
                () -> mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == snackbar);
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> mManager.dismissSnackbars(mDismissController));
        pollSnackbarCondition(
                "Snackbar did not time out", () -> !mManager.isShowing() && mDismissed);
    }

    @Test
    @SmallTest
    public void testPersistentSnackbar() throws InterruptedException {
        int timeout = 100;
        SnackbarManager.setDurationForTesting(timeout);
        final Snackbar snackbar = Snackbar.make("persistent", mDismissController,
                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR);
        mDismissed = false;
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(snackbar));
        pollSnackbarCondition("Persistent Snackbar not shown.",
                () -> mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == snackbar);
        TimeUnit.MILLISECONDS.sleep(timeout);
        pollSnackbarCondition(
                "Persistent snackbar timed out.", () -> mManager.isShowing() && !mDismissed);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.onClick(null));
        pollSnackbarCondition(
                "Persistent snackbar not removed on action.", () -> !mManager.isShowing());
    }

    @Test
    @SmallTest
    @MaxAndroidSdkLevel(value = Build.VERSION_CODES.P,
            reason = "AccessibilityManager#getRecommendedTimeoutMillis() is only available in Q+")
    public void
    testSnackbarDuration() {
        final Snackbar snackbar = Snackbar.make(
                "persistent", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            snackbar.setDuration(0);
            Assert.assertEquals(
                    "Snackbar should use default duration when client sets duration to 0.",
                    SnackbarManager.getDefaultDurationForTesting(), mManager.getDuration(snackbar));
        });

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            snackbar.setDuration(SnackbarManager.getDefaultA11yDurationForTesting() / 3);
            ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
            Assert.assertEquals(
                    "Default a11y duration should be used if the duration of snackbar in non-a11y mode is less than half of it",
                    SnackbarManager.getDefaultA11yDurationForTesting(),
                    mManager.getDuration(snackbar));
        });
    }

    @Test
    @SmallTest
    public void testOverrideParent_BeforeShowing() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.overrideParent(sAlternateParent);
            mManager.showSnackbar(snackbar);
        });
        pollSnackbarCondition("Snackbar's parent should not have been overridden, but was.",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarViewForTesting().mParent == sMainParent);
    }

    @Test
    @SmallTest
    public void testOverrideParent_WhileShowing() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.showSnackbar(snackbar);
            mManager.overrideParent(sAlternateParent);
        });
        pollSnackbarCondition("Snackbar's parent should have been overridden, but wasn't.",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarViewForTesting().mParent == sAlternateParent);
    }

    @Test
    @SmallTest
    public void testSetParent_BeforeShowing() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.setParentView(sAlternateParent);
            mManager.showSnackbar(snackbar);
        });
        pollSnackbarCondition("Snackbar's parent should have been overridden, but wasn't.",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarViewForTesting().mParent == sAlternateParent);
    }

    @Test
    @SmallTest
    public void testSetParent_WhileShowing() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.showSnackbar(snackbar);
            mManager.setParentView(sAlternateParent);
        });
        pollSnackbarCondition("Snackbar's parent should have been overridden, but wasn't.",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarViewForTesting().mParent == sAlternateParent);
    }

    @Test
    @SmallTest
    public void testSetParent_Null() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.setParentView(sAlternateParent);
            mManager.showSnackbar(snackbar);
            mManager.setParentView(null);
        });
        pollSnackbarCondition("Snackbar's parent should not have been overridden, but was.",
                ()
                        -> mManager.isShowing()
                        && mManager.getCurrentSnackbarViewForTesting().mParent == sMainParent);
    }

    @Test
    @SmallTest
    public void testSupplier_BeforeShowing() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        pollSnackbarCondition(
                "Snackbar isShowing() and isShowingSupplier().get() values are not both false before showing snackbar.",
                () -> !mManager.isShowing() && !mManager.isShowingSupplier().get());
    }

    @Test
    @SmallTest
    public void testSupplier_WhileShowing() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(snackbar));
        pollSnackbarCondition(
                "Snackbar isShowing() and isShowingSupplier().get() values are not both true while snackbar is showing.",
                () -> mManager.isShowing() && mManager.isShowingSupplier().get());
    }

    @Test
    @SmallTest
    public void testSupplier_AfterShowing() {
        final Snackbar snackbar = Snackbar.make(
                "stack", mDismissController, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(snackbar));
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> mManager.dismissSnackbars(mDismissController));
        pollSnackbarCondition(
                "Snackbar isShowing() and isShowingSupplier().get() values are not both false after dismissing snackbar.",
                () -> !mManager.isShowing() && !mManager.isShowingSupplier().get());
    }

    void pollSnackbarCondition(String message, Supplier<Boolean> condition) {
        CriteriaHelper.pollUiThread(condition::get, message);
    }
}
