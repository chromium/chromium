// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.core.app.ApplicationProvider;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.autofill.data.AuthenticatorOption;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AuthenticatorSelectionDialogBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_ENABLE_GRADIENT_GOOGLE_LOGOS,
    ChromeFeatureList.RESET_NATIVE_POINTER_IN_CREDIT_CARD_AUTH_DIALOG,
})
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

    private JniRawPtr<
                    AuthenticatorSelectionDialogBridge
                            .NativeAuthenticatorSelectionDialogViewAndroid>
            mNativeDialogView;

    private final List<AuthenticatorOption> mOptions = new ArrayList<>();

    private FakeModalDialogManager mModalDialogManager;
    private AuthenticatorSelectionDialogBridge mAuthenticatorSelectionDialogBridge;
    @Mock private AuthenticatorSelectionDialogBridge.Natives mNativeMock;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        reset(mNativeMock);
        mOptions.add(OPTION_1);
        mOptions.add(OPTION_2);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mNativeDialogView = JniRawPtr.createForTesting(100L);
        mAuthenticatorSelectionDialogBridge =
                new AuthenticatorSelectionDialogBridge(
                        mNativeDialogView,
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager);
        AuthenticatorSelectionDialogBridgeJni.setInstanceForTesting(mNativeMock);
        mAuthenticatorSelectionDialogBridge.show(mOptions);
    }

    @After
    public void tearDown() {
        if (mNativeDialogView != null) {
            mNativeDialogView.release();
        }
    }

    @Test
    public void testDismissDialog() throws Exception {
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());

        mAuthenticatorSelectionDialogBridge.dismiss();
        // Verify that no dialog is shown and that the callback is triggered on dismissal.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDismissed(mNativeDialogView);
    }

    @Test
    public void testDismissTwice() throws Exception {
        mAuthenticatorSelectionDialogBridge.dismiss();
        mAuthenticatorSelectionDialogBridge.dismiss();

        // Make sure the native side is notified only once.
        verify(mNativeMock, times(1)).onDismissed(mNativeDialogView);
    }

    @Test
    public void testOnDismissedTwice() {
        mAuthenticatorSelectionDialogBridge.dismiss();
        mAuthenticatorSelectionDialogBridge.onDialogDismissed();
        mAuthenticatorSelectionDialogBridge.onDialogDismissed();

        verify(mNativeMock, times(1)).onDismissed(mNativeDialogView);
    }

    @Test
    public void onOptionSelectedNotCalledAfterOnDismissed() {
        mAuthenticatorSelectionDialogBridge.onDialogDismissed();
        mAuthenticatorSelectionDialogBridge.onOptionSelected(OPTION_1.getIdentifier());

        verify(mNativeMock, times(1)).onDismissed(mNativeDialogView);
        verify(mNativeMock, times(0)).onOptionSelected(any(), anyString());
    }

    @Test
    public void testDismissedCalledOnPositiveButtonClick() throws Exception {
        mModalDialogManager.clickPositiveButton();

        verify(mNativeMock, times(1)).onOptionSelected(mNativeDialogView, OPTION_1.getIdentifier());
    }

    @Test
    public void testDismissedCalledOnNegativeButtonClick() throws Exception {
        mModalDialogManager.clickNegativeButton();

        verify(mNativeMock, times(1)).onDismissed(mNativeDialogView);
    }
}
