// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Holds the currently showing payment flow, of which there can be at most one per browser process.
 * Used to prevent showing more than one payment flow UI.
 *
 * <p>Not thread safe. All calls must be on the same thread: UI thread in production,
 * instrumentation thread in testing.
 *
 * <p>Sample usage:
 *
 * <pre>
 *   class PaymentRequestService {
 *       public boolean beginPaymentFlow() {
 *           return BrowserGlobalPaymentFlowManager.startPaymentFlow(this));
 *       }
 *
 *       public void invokePaymentApp(PaymentApp app) {
 *          BrowserGlobalPaymentFlowManager.initPaymentDetailsUpdateServiceHelperForInvokedApp(
 *                  this, app);
 *       }
 *
 *       public void onPaymentAppStopped() {
 *           BrowserGlobalPaymentFlowManager.onInvokedPaymentAppStopped(this);
 *       }
 *
 *       public void stopPaymentFlow() {
 *           BrowserGlobalPaymentFlowManager.onPaymentFlowStopped(this);
 *       }
 *   }
 * </pre>
 */
@NullMarked
class BrowserGlobalPaymentFlowManager {
    private static @Nullable PaymentRequestService sShowingPaymentRequest;

    /**
     * Sets the given {@link PaymentRequestService} as the currently showing payment flow for this
     * browser process, but only if no payment flow was previously showing in this browser process.
     *
     * @param paymentRequestService The payment flow that is starting.
     * @return True if no payment flow was previously showing in this browser process.
     */
    static boolean startPaymentFlow(PaymentRequestService paymentRequestService) {
        if (sShowingPaymentRequest == null) {
            sShowingPaymentRequest = paymentRequestService;
            return true;
        }

        return false;
    }

    /**
     * Initializes the payment details update service helper for the invoked payment app, if it is
     * an Android payment app.
     *
     * @param paymentRequestService The payment flow that is invoking the payment app and will
     *     listen for payment method, shipping address, and shipping option updates from the payment
     *     app. If this is not the currently shown payment flow, then this method is a no-op.
     * @param app The payment app that is being invoked.
     */
    static void initPaymentDetailsUpdateServiceHelperForInvokedApp(
            PaymentRequestService paymentRequestService, PaymentApp app) {
        if (sShowingPaymentRequest == paymentRequestService
                && app.getPaymentAppType() == PaymentAppType.NATIVE_MOBILE_APP) {
            PaymentDetailsUpdateServiceHelper.getInstance()
                    .initialize(
                            new PackageManagerDelegate(),
                            ((AndroidPaymentApp) app).packageName(),
                            /* listener= */ paymentRequestService);
        }
    }

    /**
     * Resets the dynamic price update service helper for the invoked payment app, after this app
     * stops and returns control to the browser (Clank or WebView).
     *
     * @param paymentRequestService The payment flow for which the invoked payment app has stopped.
     *     If this is not the currently shown payment flow, then this method is a no-op.
     */
    static void onInvokedPaymentAppStopped(PaymentRequestService paymentRequestService) {
        if (sShowingPaymentRequest == paymentRequestService) {
            PaymentDetailsUpdateServiceHelper.getInstance().reset();
        }
    }

    /**
     * Resets the showing payment flow, if the given {@link PaymentRequestService} is the current
     * payment flow for this browser process.
     *
     * @param paymentRequestService The payment flow that is stopping. If this is not the currently
     *     shown payment flow, then this method is a no-op.
     */
    static void onPaymentFlowStopped(PaymentRequestService paymentRequestService) {
        if (sShowingPaymentRequest == paymentRequestService) {
            sShowingPaymentRequest = null;
            PaymentDetailsUpdateServiceHelper.getInstance().reset();
        }
    }

    /** Resets the showing payment flow for testing. */
    static void resetShowingPaymentFlowForTest() {
        sShowingPaymentRequest = null;
    }

    /**
     * @return The currently showing payment flow.
     */
    static @Nullable PaymentRequestService getShowingPaymentFlow() {
        return sShowingPaymentRequest;
    }

    /**
     * @return True if a payment flow is currently showing.
     */
    static boolean isShowingPaymentFlowForTest() {
        return sShowingPaymentRequest != null;
    }

    private BrowserGlobalPaymentFlowManager() {} // Prevent instantiation.
}
