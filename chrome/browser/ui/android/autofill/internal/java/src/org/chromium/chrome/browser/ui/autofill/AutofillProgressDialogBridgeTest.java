// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/**
 * Unit tests for {@link AutofillProgressDialogBridge}
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillProgressDialogBridgeTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final String PROGRESS_DIALOG_TITLE = "Verify your card";
    private static final String PROGRESS_DIALOG_MESSAGE = "Contacting your bank...";
    private static final String PROGRESS_DIALOG_CONFIRMATION = "Your card is confirmed";
    private static final String PROGRESS_DIALOG_BUTTON_LABEL = "Cancel";
    private static final long NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW = 100L;

    @Mock
    private AutofillProgressDialogBridge.Natives mNativeMock;

    private AutofillProgressDialogBridge mAutofillProgressDialogBridge;
    private FakeModalDialogManager mModalDialogManager;

    private void showProgressDialog() {
        mAutofillProgressDialogBridge.showDialog(PROGRESS_DIALOG_TITLE, PROGRESS_DIALOG_MESSAGE,
                PROGRESS_DIALOG_BUTTON_LABEL, /* iconId= */ 0);
    }

    @Before
    public void setUp() {
        reset(mNativeMock);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mAutofillProgressDialogBridge =
                new AutofillProgressDialogBridge(NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW,
                        mModalDialogManager, ApplicationProvider.getApplicationContext());
        mMocker.mock(AutofillProgressDialogBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    public void testBasic() throws Exception {
        showProgressDialog();
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());

        mAutofillProgressDialogBridge.dismiss();
        // Verify that no dialog is shown and that the callback is triggered on dismissal.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW);
    }

    @Test
    public void testSuccessful() throws Exception {
        showProgressDialog();
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        TextView messageView = dialogView.findViewById(R.id.message);
        View progressBar = dialogView.findViewById(R.id.progress_bar);
        View confirmationIcon = dialogView.findViewById(R.id.confirmation_icon);

        Assert.assertEquals(PROGRESS_DIALOG_MESSAGE, messageView.getText());
        Assert.assertEquals(View.VISIBLE, progressBar.getVisibility());
        Assert.assertEquals(View.GONE, confirmationIcon.getVisibility());

        // Verify that the dialog is still shown.
        mAutofillProgressDialogBridge.showConfirmation(PROGRESS_DIALOG_CONFIRMATION);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());

        Assert.assertEquals(PROGRESS_DIALOG_CONFIRMATION, messageView.getText());
        Assert.assertEquals(View.GONE, progressBar.getVisibility());
        Assert.assertEquals(View.VISIBLE, confirmationIcon.getVisibility());

        mAutofillProgressDialogBridge.dismiss();
        // Verify that no dialog is shown and that the callback is triggered on dismissal.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW);
    }

    @Test
    @SmallTest
    public void testDismissedCalledOnButtonClick() throws Exception {
        showProgressDialog();

        mModalDialogManager.clickNegativeButton();

        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW);
    }
}
