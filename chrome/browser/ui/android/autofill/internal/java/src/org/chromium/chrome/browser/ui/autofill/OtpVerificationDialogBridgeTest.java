// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.core.app.ApplicationProvider;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link OtpVerificationDialogBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({AutofillFeatures.AUTOFILL_ENABLE_GRADIENT_GOOGLE_LOGOS})
public class OtpVerificationDialogBridgeTest {
    private JniRawPtr<OtpVerificationDialogBridge.NativeOtpVerificationDialogViewAndroid>
            mNativeDialogView;

    private FakeModalDialogManager mModalDialogManager;
    private OtpVerificationDialogBridge mOtpVerificationDialogBridge;
    @Mock private OtpVerificationDialogBridge.Natives mNativeMock;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mNativeDialogView = JniRawPtr.createForTesting(100L);
        mOtpVerificationDialogBridge =
                new OtpVerificationDialogBridge(
                        mNativeDialogView,
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager);
        OtpVerificationDialogBridgeJni.setInstanceForTesting(mNativeMock);
    }

    @After
    public void tearDown() {
        if (mNativeDialogView != null) {
            mNativeDialogView.release();
        }
    }

    @Test
    public void testOnConfirm_callsNative() {
        mOtpVerificationDialogBridge.onConfirm("123456");

        verify(mNativeMock, times(1)).onConfirm(mNativeDialogView, "123456");
    }

    @Test
    public void testOnNewOtpRequested_callsNative() {
        mOtpVerificationDialogBridge.onNewOtpRequested();

        verify(mNativeMock, times(1)).onNewOtpRequested(mNativeDialogView);
    }

    @Test
    public void testOnDialogDismissed_callsNativeAndClearsPointer() {
        mOtpVerificationDialogBridge.onDialogDismissed();

        verify(mNativeMock, times(1)).onDialogDismissed(mNativeDialogView);

        // Subsequent calls should not reach native.
        mOtpVerificationDialogBridge.onConfirm("123456");
        mOtpVerificationDialogBridge.onNewOtpRequested();
        mOtpVerificationDialogBridge.onDialogDismissed();

        verify(mNativeMock, never()).onConfirm(any(), anyString());
        verify(mNativeMock, never()).onNewOtpRequested(any());
        verify(mNativeMock, times(1)).onDialogDismissed(any());
    }

    @Test
    public void testNativeCallsDoNotOccurAfterDismissed() {
        mOtpVerificationDialogBridge.onDialogDismissed();

        verify(mNativeMock).onDialogDismissed(mNativeDialogView);

        mOtpVerificationDialogBridge.onConfirm("123456");
        mOtpVerificationDialogBridge.onNewOtpRequested();

        verify(mNativeMock, never()).onConfirm(any(), anyString());
        verify(mNativeMock, never()).onNewOtpRequested(any());
    }
}
