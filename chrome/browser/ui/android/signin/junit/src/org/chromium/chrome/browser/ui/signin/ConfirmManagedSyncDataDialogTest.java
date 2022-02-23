// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.widget.Button;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowAlertDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * Tests for {@link ConfirmManagedSyncDataDialog}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ConfirmManagedSyncDataDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private ConfirmManagedSyncDataDialog.Listener mMockListener;

    private FragmentActivity mActivity;

    private ConfirmSyncDataStateMachineDelegate mStateMachineDelegate;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        mStateMachineDelegate = new ConfirmSyncDataStateMachineDelegate(mActivity,
                mActivity.getSupportFragmentManager(),
                new ModalDialogManager(new AppModalPresenter(mActivity), ModalDialogType.APP));
    }

    @Test
    public void testListenerOnConfirmWhenPositiveButtonClicked() {
        AlertDialog dialog = getSignInToManagedAccountDialog();
        Button positiveButton = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
        Assert.assertEquals(
                mActivity.getString(R.string.policy_dialog_proceed), positiveButton.getText());
        positiveButton.performClick();
        verify(mMockListener).onConfirm();
    }

    @Test
    public void testListenerOnCancelWhenNegativeButtonClicked() {
        AlertDialog dialog = getSignInToManagedAccountDialog();
        Button negativeButton = dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
        Assert.assertEquals(mActivity.getString(R.string.cancel), negativeButton.getText());
        negativeButton.performClick();
        verify(mMockListener).onCancel();
    }

    @Test
    public void testListenerOnCancelNotCalledWhenDialogDismissed() {
        getSignInToManagedAccountDialog();
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        Assert.assertEquals(1, fragmentManager.getFragments().size());
        mStateMachineDelegate.dismissAllDialogs();
        Assert.assertEquals(0, fragmentManager.getFragments().size());
        verify(mMockListener, never()).onCancel();
    }

    @Test
    public void testListenerOnCancelNotCalledOnDismissWhenButtonClicked() {
        AlertDialog dialog = getSignInToManagedAccountDialog();
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        dialog.dismiss();
        verify(mMockListener, never()).onCancel();
    }

    private AlertDialog getSignInToManagedAccountDialog() {
        mStateMachineDelegate.showSignInToManagedAccountDialog(mMockListener, TEST_DOMAIN);
        return (AlertDialog) ShadowAlertDialog.getLatestDialog();
    }
}
