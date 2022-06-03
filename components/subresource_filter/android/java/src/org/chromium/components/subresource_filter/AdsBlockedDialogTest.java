// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.subresource_filter;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;

import android.content.res.Resources;
import android.text.style.ClickableSpan;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for ads blocked dialog. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdsBlockedDialogTest {
    private static final long NATIVE_PTR = 1;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ModalDialogManager mModalDialogManagerMock;

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private AdsBlockedDialog.Natives mNativeMock;

    private long mNativeDialog;
    private AdsBlockedDialog mDialog;
    private PropertyModel mModalDialogModel;
    private ClickableSpan mClickableSpan;

    @Before
    public void setUp() {
        jniMocker.mock(AdsBlockedDialogJni.TEST_HOOKS, mNativeMock);
    }

    /**
     * Tests the modal dialog properties (title, message, button text, cancel-on-touch-outside
     * setting) and verifies that the dialog is displayed as a TAB dialog.
     */
    @Test
    public void testDialogType() {
        createAndShowDialog();
        Resources resources = ApplicationProvider.getApplicationContext().getResources();

        Assert.assertEquals("Dialog title should match.",
                resources.getString(R.string.blocked_ads_dialog_title),
                mModalDialogModel.get(ModalDialogProperties.TITLE));
        Assert.assertEquals("Dialog message should match.", mDialog.getFormattedMessageText(),
                mModalDialogModel.get(ModalDialogProperties.MESSAGE));
        Assert.assertEquals("Dialog positive button text should match.",
                resources.getString(R.string.blocked_ads_dialog_always_allow),
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals("Dialog negative button text should match.",
                resources.getString(R.string.cancel),
                mModalDialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        Assert.assertTrue("Dialog should be dismissed on touch outside.",
                mModalDialogModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));

        Mockito.verify(mModalDialogManagerMock)
                .showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Tests that the dialog is dismissed when the user taps on the positive button.
     */
    @Test
    public void testDialogDismissedWithPositiveButton() {
        createAndShowDialog();
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Mockito.verify(mNativeMock).onAllowAdsClicked(anyLong());
        Mockito.verify(mNativeMock, never()).onLearnMoreClicked(anyLong());
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    /**
     * Tests that the dialog is dismissed when the user taps on the negative button.
     */
    @Test
    public void testDialogDismissedWithNegativeButton() {
        createAndShowDialog();
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Mockito.verify(mNativeMock, never()).onLearnMoreClicked(anyLong());
        Mockito.verify(mNativeMock, never()).onAllowAdsClicked(anyLong());
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    /**
     * Tests that the native #onDismissed is called when the dialog is dismissed.
     */
    @Test
    public void testDialogDismissedCallsNative() {
        createAndShowDialog();
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onDismiss(
                mModalDialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        Mockito.verify(mNativeMock).onDismissed(anyLong());
    }

    /**
     * Tests that the native #onLearnMoreClicked is called when the dialog message link
     * text is clicked.
     */
    @Test
    public void tesDialogMessageLinkCallback() {
        createAndShowDialog();
        mClickableSpan.onClick(null);
        Mockito.verify(mNativeMock).onLearnMoreClicked(anyLong());
    }

    /**
     * Helper function that creates AdsBlockedDialog, calls show() and captures the
     * property model for modal dialog view.
     */
    private void createAndShowDialog() {
        // Set nativeDialog to a non-zero value to pass assertion check
        mDialog = new AdsBlockedDialog(1, RuntimeEnvironment.application, mModalDialogManagerMock);
        mDialog.show();
        mModalDialogModel = mDialog.getDialogModelForTesting();
        mClickableSpan = mDialog.getMessageClickableSpanForTesting();
    }
}
