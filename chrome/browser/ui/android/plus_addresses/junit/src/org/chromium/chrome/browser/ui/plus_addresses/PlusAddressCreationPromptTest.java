// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlusAddressCreationPromptTest {
    private static final long NATIVE_PLUS_ADDRESS_CREATION_VIEW = 100L;
    private static final String EMAIL_PLACEHOLDER = "plus+plus@plus.plus";
    private static final String MODAL_TITLE = "mattwashere";

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private PlusAddressCreationViewBridge.Natives mPromptDelegateJni;

    private Activity mActivity;
    private FakeModalDialogManager mModalDialogManager;
    private PlusAddressCreationPrompt mPrompt;
    private PlusAddressCreationViewBridge mPromptDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.setupActivity(TestActivity.class);

        mPromptDelegate = PlusAddressCreationViewBridge.create(NATIVE_PLUS_ADDRESS_CREATION_VIEW);
        mJniMocker.mock(PlusAddressCreationViewBridgeJni.TEST_HOOKS, mPromptDelegateJni);
    }

    private void createAndShowPrompt() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mPrompt = new PlusAddressCreationPrompt(
                mPromptDelegate, mActivity, EMAIL_PLACEHOLDER, MODAL_TITLE);
        mPrompt.show(mModalDialogManager);
    }

    @Test
    @SmallTest
    public void dialogShown() {
        createAndShowPrompt();
        TextView primaryEmailView = mPrompt.getDialogViewForTesting().findViewById(
                R.id.plus_address_modal_primary_email);
        TextView modalTitleView =
                mPrompt.getDialogViewForTesting().findViewById(R.id.plus_address_notice_title);
        // Ensure that the email placeholder passed into the prompt is shown in the
        // appropriate spot.
        Assert.assertEquals(primaryEmailView.getText().toString(), EMAIL_PLACEHOLDER);
        Assert.assertEquals(modalTitleView.getText().toString(), MODAL_TITLE);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    @SmallTest
    public void positiveButtonPressed() {
        createAndShowPrompt();
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        mModalDialogManager.clickPositiveButton();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptDelegateJni, times(1))
                .onConfirmed(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
        verify(mPromptDelegateJni, times(1))
                .promptDismissed(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void negativeButtonPressed() {
        createAndShowPrompt();

        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        mModalDialogManager.clickNegativeButton();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptDelegateJni, times(1))
                .onCanceled(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
        verify(mPromptDelegateJni, times(1))
                .promptDismissed(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void dialogDismissed() {
        createAndShowPrompt();
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        mPrompt.onDismiss(null, 0);
        verify(mPromptDelegateJni, times(1))
                .promptDismissed(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }
}
