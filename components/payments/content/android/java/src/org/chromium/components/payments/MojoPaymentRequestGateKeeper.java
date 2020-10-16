// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentValidationErrors;

/**
 * Guards against invalid mojo parameters and enforces correct call sequence from mojo IPC in the
 * untrusted renderer, so PaymentRequestService does not have to.
 */
/* package */ class MojoPaymentRequestGateKeeper implements PaymentRequest {
    private final PaymentRequestServiceCreator mPaymentRequestServiceCreator;
    private PaymentRequestService mPaymentRequestService;

    /** The creator of PaymentRequestService. */
    /* package */ interface PaymentRequestServiceCreator {
        /**
         * Create an instance of PaymentRequestService if the parameters are valid.
         * @param client The client of the renderer PaymentRequest, need validation before usage.
         * @param methodData The supported methods specified by the merchant, need validation before
         *         usage.
         * @param details The payment details specified by the merchant, need validation before
         *         usage.
         * @param options The payment options specified by the merchant, need validation before
         *         usage.
         * @param googlePayBridgeEligible True when the renderer process deems the current request
         *         eligible for the skip-to-GPay experimental flow. It is ultimately up to the
         * browser process to determine whether to trigger it.
         * @param onClosedListener The listener that's invoked when PaymentRequestService has
         *         just closed.
         * @return The created instance, if the parameters are valid; otherwise, null.
         */
        PaymentRequestService createPaymentRequestServiceIfParamsValid(PaymentRequestClient client,
                PaymentMethodData[] methodData, PaymentDetails details, PaymentOptions options,
                boolean googlePayBridgeEligible, Runnable onClosedListener);
    }

    /**
     * Create an instance of MojoPaymentRequestGateKeeper.
     * @param creator The creator of PaymentRequestService.
     */
    /* package */ MojoPaymentRequestGateKeeper(PaymentRequestServiceCreator creator) {
        mPaymentRequestServiceCreator = creator;
    }

    // Implement PaymentRequest:
    @Override
    public void init(PaymentRequestClient client, PaymentMethodData[] methodData,
            PaymentDetails details, PaymentOptions options, boolean googlePayBridgeEligible) {
        if (mPaymentRequestService != null) {
            mPaymentRequestService.abortForInvalidDataFromRenderer(
                    ErrorStrings.ATTEMPTED_INITIALIZATION_TWICE);
            mPaymentRequestService = null;
            return;
        }

        // Note that a null value would be assigned when the params are invalid.
        mPaymentRequestService =
                mPaymentRequestServiceCreator.createPaymentRequestServiceIfParamsValid(client,
                        methodData, details, options, googlePayBridgeEligible,
                        this::onPaymentRequestServiceClosed);
    }

    private void onPaymentRequestServiceClosed() {
        mPaymentRequestService = null;
    }

    // Implement PaymentRequest:
    @Override
    public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.show(isUserGesture, waitForUpdatedDetails);
    }

    // Implement PaymentRequest:
    @Override
    public void updateWith(PaymentDetails details) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.updateWith(details);
    }

    // Implement PaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onPaymentDetailsNotUpdated();
    }

    // Implement PaymentRequest:
    @Override
    public void abort() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.abort();
    }

    // Implement PaymentRequest:
    @Override
    public void complete(int result) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.complete(result);
    }

    // Implement PaymentRequest:
    @Override
    public void retry(PaymentValidationErrors errors) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.retry(errors);
    }

    // Implement PaymentRequest:
    @Override
    public void canMakePayment() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.canMakePayment();
    }

    // Implement PaymentRequest:
    @Override
    public void hasEnrolledInstrument() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.hasEnrolledInstrument();
    }

    // Implement PaymentRequest:
    @Override
    public void close() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.closeByRenderer();
        mPaymentRequestService = null;
    }

    // Implement PaymentRequest:
    @Override
    public void onConnectionError(MojoException e) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onConnectionError(e);
        mPaymentRequestService = null;
    }
}
