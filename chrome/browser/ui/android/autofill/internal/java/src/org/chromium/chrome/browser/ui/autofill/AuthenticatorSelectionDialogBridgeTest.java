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
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.autofill.data.AuthenticatorOption;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AuthenticatorSelectionDialogBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK})
public class AuthenticatorSelectionDialogBridgeTest {
    // The icon set on the AuthenticatorOption is not important and any icon would do.
    private static final AuthenticatorOption OPTION_1 =
            new AuthenticatorOption.Builder()
                    .setTitle("title1")
                    .setIdentifier("identifier1")
                    .setDescription("description1")
                    .setIconResId(android.R.drawable.ic_media_pause)
                    .setType(CardUnmaskChallengeOptionType.SMS_OTP)
                    .build();

    private static final AuthenticatorOption OPTION_2 =
            new AuthenticatorOption.Builder()
                    .setTitle("title2")
                    .setIdentifier("identifier2")
                    .setDescription("description2")
                    .setIconResId(android.R.drawable.ic_media_play)
                    .setType(CardUnmaskChallengeOptionType.SMS_OTP)
                    .build();

    private static final long NATIVE_AUTHENTICATOR_SELECTION_DIALOG_VIEW = 100L;

    private List<AuthenticatorOption> mOptions = new ArrayList<>();

    private FakeModalDialogManager mModalDialogManager;
    private AuthenticatorSelectionDialogBridge mAuthenticatorSelectionDialogBridge;
    @Mock private AuthenticatorSelectionDialogBridge.Natives mNativeMock;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mMocker = new JniMocker();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        reset(mNativeMock);
        mOptions.add(OPTION_1);
        mOptions.add(OPTION_2);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mAuthenticatorSelectionDialogBridge =
                new AuthenticatorSelectionDialogBridge(
                        NATIVE_AUTHENTICATOR_SELECTION_DIALOG_VIEW,
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager);
        mMocker.mock(AuthenticatorSelectionDialogBridgeJni.TEST_HOOKS, mNativeMock);
        mAuthenticatorSelectionDialogBridge.show(mOptions);
    }

    @Test
    @SmallTest
    public void testBasic() throws Exception {
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());

        mAuthenticatorSelectionDialogBridge.dismiss();
        // Verify that no dialog is shown and that the callback is triggered on dismissal.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTHENTICATOR_SELECTION_DIALOG_VIEW);
    }

    @Test
    @SmallTest
    public void testDismissedCalledOnPositiveButtonClick() throws Exception {
        mModalDialogManager.clickPositiveButton();

        verify(mNativeMock, times(1))
                .onOptionSelected(
                        NATIVE_AUTHENTICATOR_SELECTION_DIALOG_VIEW, OPTION_1.getIdentifier());
    }

    @Test
    @SmallTest
    public void testDismissedCalledOnNegativeButtonClick() throws Exception {
        mModalDialogManager.clickNegativeButton();

        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTHENTICATOR_SELECTION_DIALOG_VIEW);
    }
}
