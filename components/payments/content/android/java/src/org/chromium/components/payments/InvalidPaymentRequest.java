// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.CanMakePaymentQueryResult;
import org.chromium.payments.mojom.HasEnrolledInstrumentQueryResult;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentValidationErrors;

/**
 * An implementation of PaymentRequest that immediately rejects all connections.
 * Necessary because Mojo does not handle null returned from createImpl().
 */
public final class InvalidPaymentRequest implements PaymentRequest {
    private PaymentRequestClient mClient;

    @Override
    public void init(
            PaymentRequestClient client,
            PaymentMethodData[] unusedMethodData,
            PaymentDetails unusedDetails,
            PaymentOptions unusedOptions) {
        mClient = client;
    }

    @Override
    public void show(boolean unusedWaitForUpdatedDetails, boolean unusedHadUserActivation) {
        if (mClient != null) {
            mClient.onError(PaymentErrorReason.USER_CANCEL, ErrorStrings.WEB_PAYMENT_API_DISABLED);
            mClient.close();
        }
    }

    @Override
    public void updateWith(PaymentDetails unusedDetails) {}

    @Override
    public void onPaymentDetailsNotUpdated() {}

    @Override
    public void abort() {}

    @Override
    public void complete(int unusedResult) {}

    @Override
    public void retry(PaymentValidationErrors unusedErrors) {}

    @Override
    public void canMakePayment() {
        if (mClient != null) {
            mClient.onCanMakePayment(CanMakePaymentQueryResult.CANNOT_MAKE_PAYMENT);
        }
    }

    @Override
    public void hasEnrolledInstrument() {
        if (mClient != null) {
            mClient.onHasEnrolledInstrument(
                    HasEnrolledInstrumentQueryResult.HAS_NO_ENROLLED_INSTRUMENT);
        }
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
