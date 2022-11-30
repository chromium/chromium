// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/**
 * Unit tests for {@link AutofillErrorDialogBridge}
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillErrorDialogBridgeTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final String ERROR_DIALOG_TITLE = "title";
    private static final String ERROR_DIALOG_DESCRIPTION = "description";
    private static final String ERROR_DIALOG_BUTTON_LABEL = "Close";
    private static final long NATIVE_AUTOFILL_ERROR_DIALOG_VIEW = 100L;

    @Mock
    private AutofillErrorDialogBridge.Natives mNativeMock;

    private AutofillErrorDialogBridge mAutofillErrorDialogBridge;
    private FakeModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        reset(mNativeMock);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mAutofillErrorDialogBridge =
                new AutofillErrorDialogBridge(NATIVE_AUTOFILL_ERROR_DIALOG_VIEW,
                        mModalDialogManager, ApplicationProvider.getApplicationContext());
        mMocker.mock(AutofillErrorDialogBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    public void testBasic() throws Exception {
        showErrorDialog();
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());

        mAutofillErrorDialogBridge.dismiss();
        // Verify that no dialog is shown and that the callback is triggered on dismissal.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_ERROR_DIALOG_VIEW);
    }

    @Test
    @SmallTest
    public void testDismissedCalledOnButtonClick() throws Exception {
        showErrorDialog();

        mModalDialogManager.clickPositiveButton();

        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_ERROR_DIALOG_VIEW);
    }

    private void showErrorDialog() {
        mAutofillErrorDialogBridge.show(ERROR_DIALOG_TITLE, ERROR_DIALOG_DESCRIPTION,
                ERROR_DIALOG_BUTTON_LABEL,
                /* iconId= */ 0);
    }
}
