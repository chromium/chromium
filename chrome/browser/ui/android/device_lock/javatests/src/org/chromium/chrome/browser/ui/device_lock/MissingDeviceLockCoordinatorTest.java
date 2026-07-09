// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.browser_ui.device_lock.DeviceLockBridge.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED;

import android.app.Activity;
import android.content.SharedPreferences;

import androidx.test.espresso.Espresso;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link MissingDeviceLockCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MissingDeviceLockCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private Activity mActivity;
    private ModalDialogManager mModalDialogManager;

    private final AtomicReference<Boolean> mOnContinueWithoutDeviceLockCalledWith =
            new AtomicReference<>();

    @Before
    public void setUpTest() {
        mActivityTestRule.setFinishActivity(true);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager =
                            new ModalDialogManager(
                                    new AppModalPresenter(mActivity), ModalDialogType.APP);
                });

        mOnContinueWithoutDeviceLockCalledWith.set(null);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mModalDialogManager != null) {
                        mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                    }
                });
        // Since the activity is launched inside this test class, we need to
        // tear it down inside the class as well.
        if (mActivity != null) {
            ApplicationTestUtils.finishActivity(mActivity);
        }
    }

    @Test
    @SmallTest
    public void testMissingDeviceLockCoordinator_showAndHideDialog() throws InterruptedException {
        HistogramWatcher dialogShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Automotive.DeviceLockRemovalDialogEvent",
                                MissingDeviceLockCoordinator.MissingDeviceLockDialogEvent
                                        .DIALOG_SHOWN)
                        .build();

        AtomicReference<MissingDeviceLockCoordinator> missingDeviceLockCoordinator =
                new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    missingDeviceLockCoordinator.set(
                            new MissingDeviceLockCoordinator(
                                    (wipeAllData) -> {}, mActivity, mModalDialogManager));
                    missingDeviceLockCoordinator.get().showDialog();
                });

        assertTrue("The modal dialog should be showing.", mModalDialogManager.isShowing());
        onView(withText(R.string.missing_device_lock_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        dialogShownHistogram.assertExpected();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        missingDeviceLockCoordinator
                                .get()
                                .hideDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));
        assertFalse(
                "The modal dialog should not be showing after hideDialog.",
                mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    public void testMissingDeviceLockCoordinator_continueWithoutDeviceLock() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().putBoolean(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, true).apply();
        HistogramWatcher continueWithoutDeviceLockHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Automotive.DeviceLockRemovalDialogEvent",
                                MissingDeviceLockCoordinator.MissingDeviceLockDialogEvent
                                        .CONTINUE_WITHOUT_DEVICE_LOCK)
                        .build();

        MissingDeviceLockCoordinator missingDeviceLockCoordinator =
                new MissingDeviceLockCoordinator(
                        (wipeAllData) -> {}, mActivity, mModalDialogManager);

        Callback<Boolean> onContinueWithoutDeviceLock = mOnContinueWithoutDeviceLockCalledWith::set;
        missingDeviceLockCoordinator.continueWithoutDeviceLock(true, onContinueWithoutDeviceLock);

        assertTrue(
                "#onContinueWithoutDeviceLock should have been called with the wipeAllData "
                        + "parameter.",
                mOnContinueWithoutDeviceLockCalledWith.get());
        assertFalse(
                "DEVICE_LOCK_PAGE_HAS_BEEN_PASSED should have been removed from the "
                        + "SharedPreferencesManager keys.",
                prefs.contains(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED));
        continueWithoutDeviceLockHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testMissingDeviceLockCoordinator_backPressDoesNotDismissDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MissingDeviceLockCoordinator missingDeviceLockCoordinator =
                            new MissingDeviceLockCoordinator(
                                    (wipeAllData) -> {}, mActivity, mModalDialogManager);
                    missingDeviceLockCoordinator.showDialog();
                });
        assertTrue("The modal dialog should be showing.", mModalDialogManager.isShowing());
        onView(withText(R.string.missing_device_lock_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        Espresso.pressBack();

        assertTrue(
                "The modal dialog should still be showing after back press.",
                mModalDialogManager.isShowing());
        onView(withText(R.string.missing_device_lock_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }
}
