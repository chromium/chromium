// Copyright 2020 The Chromium Authors
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
public class MojoPaymentRequestGateKeeper implements PaymentRequest {
    private final Delegate mDelegate;
    private PaymentRequestService mPaymentRequestService;

    /** The delegate of the class. */
    public interface Delegate {
        /**
         * Create an instance of PaymentRequestService.
         * @param client The client of the renderer PaymentRequest, need validation before usage.
         * @param onClosedListener The listener that's invoked when PaymentRequestService has
         *         just closed.
         * @return The created instance, if the parameters are valid; otherwise, null.
         */
        PaymentRequestService createPaymentRequestService(
                PaymentRequestClient client, Runnable onClosedListener);
    }

    /**
     * Create an instance of MojoPaymentRequestGateKeeper.
     * @param delegate The delegate of the instance.
     */
    public MojoPaymentRequestGateKeeper(Delegate delegate) {
        mDelegate = delegate;
    }

    // Implement PaymentRequest:
    @Override
    public void init(
            PaymentRequestClient client,
            PaymentMethodData[] methodData,
            PaymentDetails details,
            PaymentOptions options) {
        if (mPaymentRequestService != null) {
            mPaymentRequestService.abortForInvalidDataFromRenderer(
                    ErrorStrings.ATTEMPTED_INITIALIZATION_TWICE);
            mPaymentRequestService = null;
            return;
        }

        PaymentRequestService service =
                mDelegate.createPaymentRequestService(client, this::onPaymentRequestServiceClosed);
        if (!service.init(methodData, details, options)) return;
        mPaymentRequestService = service;
    }

    private void onPaymentRequestServiceClosed() {
        mPaymentRequestService = null;
    }

    // Implement PaymentRequest:
    @Override
    public void show(boolean waitForUpdatedDetails, boolean hadUserActivation) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.show(waitForUpdatedDetails, hadUserActivation);
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
