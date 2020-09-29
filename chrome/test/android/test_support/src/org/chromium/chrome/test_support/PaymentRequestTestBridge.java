// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.payments.PaymentRequestFactory;
import org.chromium.chrome.browser.payments.PaymentRequestImpl;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.ComponentPaymentRequestImpl;
import org.chromium.components.payments.ComponentPaymentRequestImpl.NativeObserverForTest;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentItem;

import java.util.List;

/**
 * Test support for injecting test behaviour from C++ tests into Java PaymentRequests.
 */
@JNINamespace("payments")
public class PaymentRequestTestBridge {
    /**
     * A test override of the PaymentRequestImpl's Delegate. Allows tests to control the answers
     * about the state of the system, in order to control which paths should be tested in the
     * PaymentRequestImpl.
     */
    private static class PaymentRequestDelegateForTest
            implements ComponentPaymentRequestImpl.Delegate {
        private final boolean mIsOffTheRecord;
        private final boolean mIsValidSsl;
        private final boolean mIsWebContentsActive;
        private final boolean mPrefsCanMakePayment;
        private final String mTwaPackageName;

        PaymentRequestDelegateForTest(boolean isOffTheRecord, boolean isValidSsl,
                boolean isWebContentsActive, boolean prefsCanMakePayment, String twaPackageName) {
            mIsOffTheRecord = isOffTheRecord;
            mIsValidSsl = isValidSsl;
            mIsWebContentsActive = isWebContentsActive;
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
        public boolean isWebContentsActive() {
            return mIsWebContentsActive;
        }

        @Override
        public boolean prefsCanMakePayment() {
            return mPrefsCanMakePayment;
        }

        @Override
        public boolean skipUiForBasicCard() {
            return false;
        }

        @Override
        @Nullable
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
        private final long mOnNotSupportedErrorPtr;
        private final long mOnConnectionTerminatedPtr;
        private final long mOnAbortCalledPtr;
        private final long mOnCompleteCalledPtr;
        private final long mOnMinimalUIReadyPtr;

        PaymentRequestNativeObserverBridgeToNativeForTest(long onCanMakePaymentCalledPtr,
                long onCanMakePaymentReturnedPtr, long onHasEnrolledInstrumentCalledPtr,
                long onHasEnrolledInstrumentReturnedPtr, long onAppListReadyPtr,
                long setAppDescriptionPtr, long onNotSupportedErrorPtr,
                long onConnectionTerminatedPtr, long onAbortCalledPtr, long onCompleteCalledPtr,
                long onMinimalUIReadyPtr) {
            mOnCanMakePaymentCalledPtr = onCanMakePaymentCalledPtr;
            mOnCanMakePaymentReturnedPtr = onCanMakePaymentReturnedPtr;
            mOnHasEnrolledInstrumentCalledPtr = onHasEnrolledInstrumentCalledPtr;
            mOnHasEnrolledInstrumentReturnedPtr = onHasEnrolledInstrumentReturnedPtr;
            mOnAppListReadyPtr = onAppListReadyPtr;
            mSetAppDescriptionsPtr = setAppDescriptionPtr;
            mOnNotSupportedErrorPtr = onNotSupportedErrorPtr;
            mOnConnectionTerminatedPtr = onConnectionTerminatedPtr;
            mOnAbortCalledPtr = onAbortCalledPtr;
            mOnCompleteCalledPtr = onCompleteCalledPtr;
            mOnMinimalUIReadyPtr = onMinimalUIReadyPtr;
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
        public void onAppListReady(@Nullable List<EditableOption> apps, PaymentItem total) {
            if (apps == null) {
                nativeSetAppDescriptions(
                        mSetAppDescriptionsPtr, new String[0], new String[0], new String[0]);
                nativeResolvePaymentRequestObserverCallback(mOnAppListReadyPtr);
                return;
            }

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
        public void onCompleteCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnCompleteCalledPtr);
        }
        @Override
        public void onMinimalUIReady() {
            nativeResolvePaymentRequestObserverCallback(mOnMinimalUIReadyPtr);
        }
    }

    private static final String TAG = "PaymentRequestTestBridge";

    @CalledByNative
    private static void setUseDelegateForTest(boolean useDelegate, boolean isOffTheRecord,
            boolean isValidSsl, boolean isWebContentsActive, boolean prefsCanMakePayment,
            boolean skipUiForBasicCard, String twaPackageName) {
        if (useDelegate) {
            PaymentRequestFactory.sDelegateForTest =
                    new PaymentRequestDelegateForTest(isOffTheRecord, isValidSsl,
                            isWebContentsActive, prefsCanMakePayment, twaPackageName);
        } else {
            PaymentRequestFactory.sDelegateForTest = null;
        }
    }

    @CalledByNative
    private static void setUseNativeObserverForTest(long onCanMakePaymentCalledPtr,
            long onCanMakePaymentReturnedPtr, long onHasEnrolledInstrumentCalledPtr,
            long onHasEnrolledInstrumentReturnedPtr, long onAppListReadyPtr,
            long setAppDescriptionPtr, long onNotSupportedErrorPtr, long onConnectionTerminatedPtr,
            long onAbortCalledPtr, long onCompleteCalledPtr, long onMinimalUIReadyPtr) {
        ComponentPaymentRequestImpl.setNativeObserverForTest(
                new PaymentRequestNativeObserverBridgeToNativeForTest(onCanMakePaymentCalledPtr,
                        onCanMakePaymentReturnedPtr, onHasEnrolledInstrumentCalledPtr,
                        onHasEnrolledInstrumentReturnedPtr, onAppListReadyPtr, setAppDescriptionPtr,
                        onNotSupportedErrorPtr, onConnectionTerminatedPtr, onAbortCalledPtr,
                        onCompleteCalledPtr, onMinimalUIReadyPtr));
    }

    @CalledByNative
    private static WebContents getPaymentHandlerWebContentsForTest() {
        return PaymentRequestImpl.getPaymentHandlerWebContentsForTest();
    }

    @CalledByNative
    private static boolean clickPaymentHandlerSecurityIconForTest() {
        return PaymentRequestImpl.clickPaymentHandlerSecurityIconForTest();
    }

    @CalledByNative
    private static boolean clickPaymentHandlerCloseButtonForTest() {
        return PaymentRequestImpl.clickPaymentHandlerCloseButtonForTest();
    }

    @CalledByNative
    private static boolean confirmMinimalUIForTest() {
        return PaymentRequestImpl.confirmMinimalUIForTest();
    }

    @CalledByNative
    private static boolean dismissMinimalUIForTest() {
        return PaymentRequestImpl.dismissMinimalUIForTest();
    }

    @CalledByNative
    private static boolean isAndroidMarshmallowOrLollipopForTest() {
        return Build.VERSION.SDK_INT == Build.VERSION_CODES.M
                || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP
                || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP_MR1;
    }

    /**
     * The native method responsible to executing RepeatingCallback pointers.
     */
    private static native void nativeResolvePaymentRequestObserverCallback(long callbackPtr);

    private static native void nativeSetAppDescriptions(
            long callbackPtr, String[] appLabels, String[] appSublabels, String[] appTotals);
}
