// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;

/** Native bridge for finding payment apps. */
public class PaymentAppServiceBridge implements PaymentAppFactoryInterface {
    private static boolean sCanMakePaymentForTesting;

    public PaymentAppServiceBridge() {}

    /**
     * Make canMakePayment() return true always for testing purpose.
     *
     * @param canMakePayment Indicates whether a SW payment app can make payment.
     */
    public static void setCanMakePaymentForTesting(boolean canMakePayment) {
        sCanMakePaymentForTesting = canMakePayment;
        ResettersForTesting.register(() -> sCanMakePaymentForTesting = false);
    }

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        if (delegate.getParams().hasClosed()
                || delegate.getParams().getRenderFrameHost().getLastCommittedURL() == null
                || delegate.getParams().getRenderFrameHost().getLastCommittedOrigin() == null
                || delegate.getParams().getWebContents().isDestroyed()) {
            return;
        }

        assert delegate.getParams()
                .getPaymentRequestOrigin()
                .equals(
                        UrlFormatter.formatUrlForSecurityDisplay(
                                delegate.getParams().getRenderFrameHost().getLastCommittedURL(),
                                SchemeDisplay.SHOW));

        CSPCheckerBridge cspCheckerBridge = new CSPCheckerBridge(delegate.getCSPChecker());

        PaymentAppServiceCallback callback =
                new PaymentAppServiceCallback(delegate, cspCheckerBridge);

        PaymentAppServiceBridgeJni.get()
                .create(
                        delegate.getParams().getRenderFrameHost(),
                        delegate.getParams().getTopLevelOrigin(),
                        delegate.getParams().getSpec(),
                        delegate.getParams().getTwaPackageName(),
                        delegate.getParams().getMayCrawl(),
                        delegate.getParams().isOffTheRecord(),
                        cspCheckerBridge.getNativeCSPChecker(),
                        callback);
    }

    /** Handles callbacks from native PaymentAppService. */
    public class PaymentAppServiceCallback {
        private final PaymentAppFactoryDelegate mDelegate;
        private final CSPCheckerBridge mCSPCheckerBridge;

        private PaymentAppServiceCallback(
                PaymentAppFactoryDelegate delegate, CSPCheckerBridge cspCheckerBridge) {
            mDelegate = delegate;
            mCSPCheckerBridge = cspCheckerBridge;
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onCanMakePaymentCalculated(boolean canMakePayment) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onCanMakePaymentCalculated(canMakePayment || sCanMakePaymentForTesting);
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreated(PaymentApp paymentApp) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreated(paymentApp);
        }

        /**
         * Called when an error has occurred.
         * @param errorMessage Developer facing error message.
         * @param errorReason Internal reason for the error.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreationError(
                String errorMessage, @AppCreationFailureReason int errorReason) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreationError(errorMessage, errorReason);
        }

        /**
         * Called when the factory is finished creating payment apps. Expects to be called exactly
         * once and after all onPaymentAppCreated() calls.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onDoneCreatingPaymentApps() {
            ThreadUtils.assertOnUiThread();
            mCSPCheckerBridge.destroy();
            mDelegate.onDoneCreatingPaymentApps(PaymentAppServiceBridge.this);
        }

        /**
         * Forces canMakePayment() and hasEnrolledInstrument() to return true even when no payment
         * app is created.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void setCanMakePaymentEvenWithoutApps() {
            ThreadUtils.assertOnUiThread();
            mDelegate.setCanMakePaymentEvenWithoutApps();
        }

        /**
         * Records that an Opt Out experience will be offered to the user in the
         * current UI flow.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void setOptOutOffered() {
            ThreadUtils.assertOnUiThread();
            mDelegate.setOptOutOffered();
        }
    }

    @NativeMethods
    /* package */ interface Natives {
        /**
         * Creates a native payment app service.
         * @param initiatorRenderFrameHost The host of the render frame where PaymentRequest API was
         * invoked.
         * @param topOrigin The (scheme, host, port) tuple of top level context where
         * PaymentRequest API was invoked.
         * @param spec The parameters passed into the PaymentRequest API.
         * @param twaPackageName The Android package name of the Trusted Web Activity that invoked
         * Chrome. If not running in TWA mode, then this string is null or empty.
         * @param mayCrawlForInstallablePaymentApps Whether crawling for just-in-time installable
         * payment apps is allowed.
         * @param isOffTheRecord Whether the merchant WebContent's profile is in off-the-record
         * mode.
         * @param nativeCSPCheckerAndroid A C++ native CSPCheckerAndroid* pointer.
         * @param callback The callback that receives the discovered payment apps.
         */
        void create(
                RenderFrameHost initiatorRenderFrameHost,
                String topOrigin,
                PaymentRequestSpec spec,
                String twaPackageName,
                boolean mayCrawlForInstallablePaymentApps,
                boolean isOffTheRecord,
                long nativeCSPCheckerAndroid,
                PaymentAppServiceCallback callback);
    }
}
