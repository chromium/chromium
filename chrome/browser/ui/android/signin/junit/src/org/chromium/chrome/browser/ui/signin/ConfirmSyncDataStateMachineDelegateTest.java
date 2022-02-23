// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowAlertDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/** Tests for {@link ConfirmSyncDataStateMachineDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class ConfirmSyncDataStateMachineDelegateTest {
    private FragmentManager mFragmentManager;
    private ConfirmSyncDataStateMachineDelegate mStateMachineDelegate;

    @Before
    public void setUp() {
        FragmentActivity activity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        mFragmentManager = activity.getSupportFragmentManager();
        mStateMachineDelegate = new ConfirmSyncDataStateMachineDelegate(activity, mFragmentManager,
                new ModalDialogManager(new AppModalPresenter(activity), ModalDialogType.APP));
    }

    @Test
    public void testTimeoutDialogWhenPositiveButtonPressed() {
        ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(mockListener);
        AlertDialog alertDialog = (AlertDialog) ShadowAlertDialog.getLatestDialog();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mockListener).onRetry();
    }

    @Test
    public void testTimeoutDialogWhenNegativeButtonPressed() {
        ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(mockListener);
        AlertDialog alertDialog = (AlertDialog) ShadowAlertDialog.getLatestDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mockListener).onCancel();
    }

    @Test
    public void testProgressDialog() {
        ConfirmSyncDataStateMachineDelegate.ProgressDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.ProgressDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyProgressDialog(mockListener);
        AlertDialog alertDialog = (AlertDialog) ShadowAlertDialog.getLatestDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mockListener).onCancel();
    }

    @Test
    public void testDismissAllDialogs() {
        ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(mockListener);
        Assert.assertEquals(1, mFragmentManager.getFragments().size());
        mStateMachineDelegate.dismissAllDialogs();
        Assert.assertEquals(0, mFragmentManager.getFragments().size());
    }
}
