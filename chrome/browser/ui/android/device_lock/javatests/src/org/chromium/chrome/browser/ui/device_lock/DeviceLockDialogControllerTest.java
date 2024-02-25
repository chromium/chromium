// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link DeviceLockDialogController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DeviceLockDialogControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private Activity mActivity;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private View mView;
    @Mock private ReauthenticatorBridge mReauthenticatorBridge;

    private AtomicBoolean mDeviceLockReady = new AtomicBoolean();
    private AtomicBoolean mDeviceLockRefused = new AtomicBoolean();

    @Before
    public void setUpTest() {
        mActivity = Mockito.mock(Activity.class);
        mModalDialogManager = Mockito.mock(ModalDialogManager.class);
        mReauthenticatorBridge = Mockito.mock(ReauthenticatorBridge.class);
        mView = Mockito.mock(View.class);
        mActivityTestRule.setFinishActivity(true);

        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);

        mDeviceLockReady.set(false);
        mDeviceLockRefused.set(false);
    }

    @After
    public void tearDown() throws Exception {
        // Since the activity is launched inside this test class, we need to
        // tear it down inside the class as well.
        if (mActivity != null) {
            ApplicationTestUtils.finishActivity(mActivity);
        }
    }

    @Test
    @SmallTest
    public void testDeviceLockLauncher_showAndHideDialog() {
        DeviceLockDialogController deviceLockDialogController =
                new DeviceLockDialogController(
                        () -> mDeviceLockReady.set(true),
                        () -> mDeviceLockRefused.set(true),
                        null,
                        mActivity,
                        mModalDialogManager,
                        null,
                        true,
                        DeviceLockActivityLauncher.Source.AUTOFILL);
        assertNotNull("The Device Lock launcher should not be null.", deviceLockDialogController);

        deviceLockDialogController.showDialog();
        verify(mModalDialogManager, times(1)).showDialog(any(), anyInt(), anyInt());

        deviceLockDialogController.hideDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mModalDialogManager, times(1))
                .dismissDialog(any(), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));
    }

    @Test
    @SmallTest
    public void testDeviceLockLauncher_showAndHideDialog_deviceLockReauthenticationNotRequired() {
        DeviceLockDialogController deviceLockDialogController =
                new DeviceLockDialogController(
                        () -> mDeviceLockReady.set(true),
                        () -> mDeviceLockRefused.set(true),
                        null,
                        mActivity,
                        mModalDialogManager,
                        null,
                        false,
                        DeviceLockActivityLauncher.Source.AUTOFILL);
        assertNotNull("The Device Lock launcher should not be null.", deviceLockDialogController);

        deviceLockDialogController.showDialog();
        verify(mModalDialogManager, times(1)).showDialog(any(), anyInt(), anyInt());

        deviceLockDialogController.hideDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mModalDialogManager, times(1))
                .dismissDialog(any(), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));
    }

    @Test
    @SmallTest
    public void testDeviceLockLauncher_onDeviceLockReady() {
        DeviceLockDialogController deviceLockDialogController =
                new DeviceLockDialogController(
                        () -> mDeviceLockReady.set(true),
                        () -> mDeviceLockRefused.set(true),
                        null,
                        mActivity,
                        mModalDialogManager,
                        null,
                        true,
                        DeviceLockActivityLauncher.Source.AUTOFILL);
        assertNotNull("The Device Lock launcher should not be null.", deviceLockDialogController);

        deviceLockDialogController.setView(mView);
        deviceLockDialogController.onDeviceLockReady();
        assertTrue(
                "The launcher's #onDeviceLockReady should have been called.",
                mDeviceLockReady.get());
        assertFalse(
                "The launcher's #onDeviceLockRefused should not have been called.",
                mDeviceLockRefused.get());
    }

    @Test
    @SmallTest
    public void testDeviceLockLauncher_onDeviceLockRefused() {
        DeviceLockDialogController deviceLockDialogController =
                new DeviceLockDialogController(
                        () -> mDeviceLockReady.set(true),
                        () -> mDeviceLockRefused.set(true),
                        null,
                        mActivity,
                        mModalDialogManager,
                        null,
                        true,
                        DeviceLockActivityLauncher.Source.AUTOFILL);
        assertNotNull("The Device Lock launcher should not be null.", deviceLockDialogController);

        deviceLockDialogController.setView(mView);
        deviceLockDialogController.onDeviceLockRefused();
        assertFalse(
                "The launcher's #onDeviceLockReady should not have been called.",
                mDeviceLockReady.get());
        assertTrue(
                "The launcher's #onDeviceLockRefused should have been called.",
                mDeviceLockRefused.get());
    }
}
