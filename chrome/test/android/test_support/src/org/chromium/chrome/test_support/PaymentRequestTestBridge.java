// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.payments.ChromePaymentRequestFactory;
import org.chromium.chrome.browser.payments.ChromePaymentRequestService;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestService.NativeObserverForTest;
import org.chromium.components.payments.PaymentUiServiceTestInterface;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationAuthnController;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationNoMatchingCredController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentItem;

import java.util.List;

/**
 * Test support for injecting test behaviour from C++ tests into Java PaymentRequests.
 */
@JNINamespace("payments")
public class PaymentRequestTestBridge {
    private static PaymentUiServiceTestInterface sUiService;

    /**
     * A test override of the ChromePaymentRequestService's Delegate. Allows tests to control the
     * answers about the state of the system, in order to control which paths should be tested in
     * the ChromePaymentRequestService.
     */
    private static class ChromePaymentRequestDelegateForTest
            extends PaymentRequestDelegateForTest implements ChromePaymentRequestService.Delegate {
        ChromePaymentRequestDelegateForTest(boolean isOffTheRecord, boolean isValidSsl,
                boolean prefsCanMakePayment, String twaPackageName) {
            super(isOffTheRecord, isValidSsl, prefsCanMakePayment, twaPackageName);
        }

        @Override
        public BrowserPaymentRequest createBrowserPaymentRequest(
                PaymentRequestService paymentRequestService) {
            return new ChromePaymentRequestService(paymentRequestService, this);
        }
    }

    /**
     * A test override of the PaymentRequestService's Delegate. Allows tests to control the
     * answers about the state of the system, in order to control which paths should be tested in
     * the ChromePaymentRequestService.
     */
    private abstract static class PaymentRequestDelegateForTest
            implements PaymentRequestService.Delegate {
        private final boolean mIsOffTheRecord;
        private final boolean mIsValidSsl;
        private final boolean mPrefsCanMakePayment;
        private final String mTwaPackageName;

        PaymentRequestDelegateForTest(boolean isOffTheRecord, boolean isValidSsl,
                boolean prefsCanMakePayment, String twaPackageName) {
            mIsOffTheRecord = isOffTheRecord;
            mIsValidSsl = isValidSsl;
            mPrefsCanMakePayment = prefsCanMakePayment;
            mTwaPackageName = twaPackageName;
        }

        @Override
        public boolean isOffTheRecord() {
            return mIsOffTheRecord;
        }

        @Override
        public String getInvalidSslCertificateErrorMessage() {
            if (mIsValidSsl) return null;
            return "Invalid SSL certificate";
        }

        @Override
        public boolean prefsCanMakePayment() {
            return mPrefsCanMakePayment;
        }

        @Nullable
        @Override
        public String getTwaPackageName() {
            return mTwaPackageName;
        }
    }

    /**
     * Implements NativeObserverForTest by holding pointers to C++ callbacks, and invoking
     * them through nativeResolvePaymentRequestObserverCallback() when the observer's
     * methods are called.
     */
    private static class PaymentRequestNativeObserverBridgeToNativeForTest
            implements NativeObserverForTest {
        private final long mOnCanMakePaymentCalledPtr;
        private final long mOnCanMakePaymentReturnedPtr;
        private final long mOnHasEnrolledInstrumentCalledPtr;
        private final long mOnHasEnrolledInstrumentReturnedPtr;
        private final long mOnAppListReadyPtr;
        private final long mSetAppDescriptionsPtr;
        private final long mOnErrorDisplayedPtr;
        private final long mOnNotSupportedErrorPtr;
        private final long mOnConnectionTerminatedPtr;
        private final long mOnAbortCalledPtr;
        private final long mOnCompleteHandledPtr;
        private final long mOnUiDisplayed;

        PaymentRequestNativeObserverBridgeToNativeForTest(long onCanMakePaymentCalledPtr,
                long onCanMakePaymentReturnedPtr, long onHasEnrolledInstrumentCalledPtr,
                long onHasEnrolledInstrumentReturnedPtr, long onAppListReadyPtr,
                long setAppDescriptionPtr, long onErrorDisplayedPtr, long onNotSupportedErrorPtr,
                long onConnectionTerminatedPtr, long onAbortCalledPtr, long onCompleteHandledPtr,
                long onUiDisplayed) {
            mOnCanMakePaymentCalledPtr = onCanMakePaymentCalledPtr;
            mOnCanMakePaymentReturnedPtr = onCanMakePaymentReturnedPtr;
            mOnHasEnrolledInstrumentCalledPtr = onHasEnrolledInstrumentCalledPtr;
            mOnHasEnrolledInstrumentReturnedPtr = onHasEnrolledInstrumentReturnedPtr;
            mOnAppListReadyPtr = onAppListReadyPtr;
            mSetAppDescriptionsPtr = setAppDescriptionPtr;
            mOnErrorDisplayedPtr = onErrorDisplayedPtr;
            mOnNotSupportedErrorPtr = onNotSupportedErrorPtr;
            mOnConnectionTerminatedPtr = onConnectionTerminatedPtr;
            mOnAbortCalledPtr = onAbortCalledPtr;
            mOnCompleteHandledPtr = onCompleteHandledPtr;
            mOnUiDisplayed = onUiDisplayed;
        }

        @Override
        public void onPaymentUiServiceCreated(PaymentUiServiceTestInterface uiService) {
            assert uiService != null;
            sUiService = uiService;
        }

        @Override
        public void onClosed() {
            sUiService = null;
        }

        @Override
        public void onCanMakePaymentCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnCanMakePaymentCalledPtr);
        }
        @Override
        public void onCanMakePaymentReturned() {
            nativeResolvePaymentRequestObserverCallback(mOnCanMakePaymentReturnedPtr);
        }
        @Override
        public void onHasEnrolledInstrumentCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnHasEnrolledInstrumentCalledPtr);
        }
        @Override
        public void onHasEnrolledInstrumentReturned() {
            nativeResolvePaymentRequestObserverCallback(mOnHasEnrolledInstrumentReturnedPtr);
        }

        @Override
        public void onAppListReady(List<PaymentApp> apps, PaymentItem total) {
            String[] appLabels = new String[apps.size()];
            String[] appSublabels = new String[apps.size()];
            String[] appTotals = new String[apps.size()];

            for (int i = 0; i < apps.size(); i++) {
                EditableOption app = apps.get(i);
                appLabels[i] = ensureNotNull(app.getLabel());
                appSublabels[i] = ensureNotNull(app.getSublabel());
                if (!TextUtils.isEmpty(app.getPromoMessage())) {
                    appTotals[i] = app.getPromoMessage();
                } else {
                    appTotals[i] = total.amount.currency + " " + total.amount.value;
                }
            }

            nativeSetAppDescriptions(mSetAppDescriptionsPtr, appLabels, appSublabels, appTotals);
            nativeResolvePaymentRequestObserverCallback(mOnAppListReadyPtr);
        }

        @Override
        public void onErrorDisplayed() {
            nativeResolvePaymentRequestObserverCallback(mOnErrorDisplayedPtr);
        }

        private static String ensureNotNull(@Nullable String value) {
            return value == null ? "" : value;
        }

        @Override
        public void onNotSupportedError() {
            nativeResolvePaymentRequestObserverCallback(mOnNotSupportedErrorPtr);
        }
        @Override
        public void onConnectionTerminated() {
            nativeResolvePaymentRequestObserverCallback(mOnConnectionTerminatedPtr);
        }
        @Override
        public void onAbortCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnAbortCalledPtr);
        }
        @Override
        public void onCompleteHandled() {
            nativeResolvePaymentRequestObserverCallback(mOnCompleteHandledPtr);
        }
        @Override
        public void onUiDisplayed() {
            nativeResolvePaymentRequestObserverCallback(mOnUiDisplayed);
        }
    }

    private static final String TAG = "PaymentRequestTestBridge";

    @CalledByNative
    private static void setUseDelegateForTest(boolean isOffTheRecord, boolean isValidSsl,
            boolean prefsCanMakePayment, String twaPackageName) {
        ChromePaymentRequestFactory.sDelegateForTest = new ChromePaymentRequestDelegateForTest(
                isOffTheRecord, isValidSsl, prefsCanMakePayment, twaPackageName);
    }

    @CalledByNative
    private static void setUseNativeObserverForTest(long onCanMakePaymentCalledPtr,
            long onCanMakePaymentReturnedPtr, long onHasEnrolledInstrumentCalledPtr,
            long onHasEnrolledInstrumentReturnedPtr, long onAppListReadyPtr,
            long setAppDescriptionPtr, long onErrorDisplayedPtr, long onNotSupportedErrorPtr,
            long onConnectionTerminatedPtr, long onAbortCalledPtr, long onCompleteCalledPtr,
            long onUiDisplayedPtr) {
        PaymentRequestService.setNativeObserverForTest(
                new PaymentRequestNativeObserverBridgeToNativeForTest(onCanMakePaymentCalledPtr,
                        onCanMakePaymentReturnedPtr, onHasEnrolledInstrumentCalledPtr,
                        onHasEnrolledInstrumentReturnedPtr, onAppListReadyPtr, setAppDescriptionPtr,
                        onErrorDisplayedPtr, onNotSupportedErrorPtr, onConnectionTerminatedPtr,
                        onAbortCalledPtr, onCompleteCalledPtr, onUiDisplayedPtr));
    }

    @CalledByNative
    private static WebContents getPaymentHandlerWebContentsForTest() {
        return sUiService.getPaymentHandlerWebContentsForTest();
    }

    @CalledByNative
    private static boolean clickPaymentHandlerSecurityIconForTest() {
        return sUiService.clickPaymentHandlerSecurityIconForTest();
    }

    @CalledByNative
    private static boolean clickPaymentHandlerCloseButtonForTest() {
        return sUiService.clickPaymentHandlerCloseButtonForTest();
    }

    @CalledByNative
    private static boolean closeDialogForTest() {
        SecurePaymentConfirmationAuthnController authnUi =
                PaymentRequestService.getSecurePaymentConfirmationAuthnUiForTesting();
        if (authnUi != null) return authnUi.cancelForTest();

        SecurePaymentConfirmationNoMatchingCredController noMatchingUi =
                PaymentRequestService.getSecurePaymentConfirmationNoMatchingCredUiForTesting();
        if (noMatchingUi != null) {
            noMatchingUi.close();
            return true;
        }

        return sUiService == null || sUiService.closeDialogForTest();
    }

    @CalledByNative
    private static boolean clickSecurePaymentConfirmationOptOutForTest() {
        SecurePaymentConfirmationAuthnController authnUi =
                PaymentRequestService.getSecurePaymentConfirmationAuthnUiForTesting();
        if (authnUi != null) return authnUi.optOutForTest();
        SecurePaymentConfirmationNoMatchingCredController noMatchingUi =
                PaymentRequestService.getSecurePaymentConfirmationNoMatchingCredUiForTesting();
        if (noMatchingUi != null) return noMatchingUi.optOutForTest();
        return false;
    }

    /**
     * The native method responsible to executing RepeatingCallback pointers.
     */
    private static native void nativeResolvePaymentRequestObserverCallback(long callbackPtr);

    private static native void nativeSetAppDescriptions(
            long callbackPtr, String[] appLabels, String[] appSublabels, String[] appTotals);
}
