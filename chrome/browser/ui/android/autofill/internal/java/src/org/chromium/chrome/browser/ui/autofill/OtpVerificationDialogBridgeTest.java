// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link OtpVerificationDialogBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OtpVerificationDialogBridgeTest {
    private static final long NATIVE_OTP_VERIFICATION_DIALOG_VIEW = 100L;

    private FakeModalDialogManager mModalDialogManager;
    private OtpVerificationDialogBridge mOtpVerificationDialogBridge;
    @Mock private OtpVerificationDialogBridge.Natives mNativeMock;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mOtpVerificationDialogBridge =
                new OtpVerificationDialogBridge(
                        NATIVE_OTP_VERIFICATION_DIALOG_VIEW,
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager);
        OtpVerificationDialogBridgeJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    @SmallTest
    public void testOnConfirm_callsNative() {
        mOtpVerificationDialogBridge.onConfirm("123456");

        verify(mNativeMock, times(1)).onConfirm(NATIVE_OTP_VERIFICATION_DIALOG_VIEW, "123456");
    }

    @Test
    @SmallTest
    public void testOnNewOtpRequested_callsNative() {
        mOtpVerificationDialogBridge.onNewOtpRequested();

        verify(mNativeMock, times(1)).onNewOtpRequested(NATIVE_OTP_VERIFICATION_DIALOG_VIEW);
    }

    @Test
    @SmallTest
    public void testOnDialogDismissed_callsNativeAndClearsPointer() {
        mOtpVerificationDialogBridge.onDialogDismissed();

        verify(mNativeMock, times(1)).onDialogDismissed(NATIVE_OTP_VERIFICATION_DIALOG_VIEW);

        // Subsequent calls should not reach native.
        mOtpVerificationDialogBridge.onConfirm("123456");
        mOtpVerificationDialogBridge.onNewOtpRequested();
        mOtpVerificationDialogBridge.onDialogDismissed();

        verify(mNativeMock, never()).onConfirm(anyLong(), anyString());
        verify(mNativeMock, never()).onNewOtpRequested(anyLong());
        verify(mNativeMock, times(1)).onDialogDismissed(anyLong());
    }

    @Test
    @SmallTest
    public void testNativeCallsDoNotOccurAfterDismissed() {
        mOtpVerificationDialogBridge.onDialogDismissed();

        verify(mNativeMock).onDialogDismissed(NATIVE_OTP_VERIFICATION_DIALOG_VIEW);

        mOtpVerificationDialogBridge.onConfirm("123456");
        mOtpVerificationDialogBridge.onNewOtpRequested();

        verify(mNativeMock, never()).onConfirm(anyLong(), anyString());
        verify(mNativeMock, never()).onNewOtpRequested(anyLong());
    }
}
