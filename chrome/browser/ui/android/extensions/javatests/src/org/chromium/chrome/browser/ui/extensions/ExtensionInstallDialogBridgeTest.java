// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;
import android.graphics.Bitmap;

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
import org.chromium.chrome.browser.ui.extensions.ExtensionInstallDialogBridge.Natives;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link ExtensionInstallDialogBridge} */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionInstallDialogBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TITLE = "Add 'extension name'?";
    private static final String ACCEPT_BUTTON_LABEL = "Add extension";
    private static final String CANCEL_BUTTON_LABEL = "Cancel";
    private static final long NATIVE_INSTALL_EXTENSION_DIALOG_VIEW = 100L;
    private static final Bitmap ICON = Bitmap.createBitmap(24, 24, Bitmap.Config.ARGB_8888);

    @Mock private Natives mNativeMock;

    private FakeModalDialogManager mModalDialogManager;
    private Resources mResources;
    private ExtensionInstallDialogBridge mExtensionInstallDialogBridge;

    private void showExtensionInstallDialog() {
        mExtensionInstallDialogBridge.showDialog(
                TITLE, ICON, ACCEPT_BUTTON_LABEL, CANCEL_BUTTON_LABEL);
    }

    @Before
    public void setUp() {
        reset(mNativeMock);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mResources = ApplicationProvider.getApplicationContext().getResources();
        mExtensionInstallDialogBridge =
                new ExtensionInstallDialogBridge(
                        NATIVE_INSTALL_EXTENSION_DIALOG_VIEW,
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager);
        ExtensionInstallDialogBridgeJni.setInstanceForTesting(mNativeMock);
    }

    /**
     * Tests that clicking on the dialog's cancel button triggers the onDialogCanceled() and
     * destroy() callbacks.
     */
    @Test
    @SmallTest
    public void testOnAcceptButtonClicked() throws Exception {
        showExtensionInstallDialog();

        mModalDialogManager.clickPositiveButton();

        verify(mNativeMock, times(1)).onDialogAccepted(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
        verify(mNativeMock, times(1)).destroy(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
    }

    /**
     * Tests that clicking on the dialog's accept button triggers the onDialogAccepted() and
     * destroy() callbacks.
     */
    @Test
    @SmallTest
    public void testOnCancelButtonClicked() throws Exception {
        showExtensionInstallDialog();

        mModalDialogManager.clickNegativeButton();

        verify(mNativeMock, times(1)).onDialogCanceled(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
        verify(mNativeMock, times(1)).destroy(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
    }

    /**
     * Tests that dismissing the dialog (without clicking any of the buttons) triggers the
     * onDialogDismissed() and destroy() callbacks.
     */
    @Test
    @SmallTest
    public void testOnDialogDismissed() throws Exception {
        showExtensionInstallDialog();

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(model);
        mModalDialogManager.dismissDialog(model, DialogDismissalCause.UNKNOWN);

        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDialogDismissed(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
        verify(mNativeMock, times(1)).destroy(NATIVE_INSTALL_EXTENSION_DIALOG_VIEW);
    }
}
