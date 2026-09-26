// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.browser_ui.device_lock.DeviceLockBridge.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED;

import android.app.Activity;
import android.content.SharedPreferences;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/** Tests for {@link MissingDeviceLockCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MissingDeviceLockCoordinatorTest {
    private Activity mActivity;
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUpTest() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModalDialogManager =
                new ModalDialogManager(new AppModalPresenter(mActivity), ModalDialogType.APP);
    }

    @After
    public void tearDown() {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
        }
    }

    @Test
    public void testMissingDeviceLockCoordinator_showAndHideDialog() {
        HistogramWatcher dialogShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Automotive.DeviceLockRemovalDialogEvent",
                                MissingDeviceLockCoordinator.MissingDeviceLockDialogEvent
                                        .DIALOG_SHOWN)
                        .build();

        MissingDeviceLockCoordinator missingDeviceLockCoordinator =
                new MissingDeviceLockCoordinator(
                        CallbackUtils.emptyCallback(), mActivity, mModalDialogManager);
        missingDeviceLockCoordinator.showDialog();

        assertTrue("The modal dialog should be showing.", mModalDialogManager.isShowing());
        assertTrue(mModalDialogManager.isShowing());

        dialogShownHistogram.assertExpected();
        missingDeviceLockCoordinator.hideDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        assertFalse(
                "The modal dialog should not be showing after hideDialog.",
                mModalDialogManager.isShowing());
    }

    @Test
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
                        CallbackUtils.emptyCallback(), mActivity, mModalDialogManager);

        PayloadCallbackHelper<Boolean> continueWithoutDeviceLockHelper =
                new PayloadCallbackHelper<>();
        missingDeviceLockCoordinator.continueWithoutDeviceLock(
                true, continueWithoutDeviceLockHelper::notifyCalled);

        assertTrue(
                "#onContinueWithoutDeviceLock should have been called with the wipeAllData "
                        + "parameter.",
                continueWithoutDeviceLockHelper.getOnlyPayloadBlocking());
        assertFalse(
                "DEVICE_LOCK_PAGE_HAS_BEEN_PASSED should have been removed from the "
                        + "SharedPreferencesManager keys.",
                prefs.contains(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED));
        continueWithoutDeviceLockHistogram.assertExpected();
    }

    @Test
    public void testMissingDeviceLockCoordinator_backPressDoesNotDismissDialog() {
        MissingDeviceLockCoordinator missingDeviceLockCoordinator =
                new MissingDeviceLockCoordinator(
                        CallbackUtils.emptyCallback(), mActivity, mModalDialogManager);
        missingDeviceLockCoordinator.showDialog();

        assertTrue("The modal dialog should be showing.", mModalDialogManager.isShowing());
        assertTrue(mModalDialogManager.isShowing());

        mActivity.onBackPressed();

        assertTrue(
                "The modal dialog should still be showing after back press.",
                mModalDialogManager.isShowing());
        assertTrue(mModalDialogManager.isShowing());
    }
}
