// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;
import org.chromium.payments.mojom.PaymentErrorReason;

/** The error of payment UIs not being shown. */
@NullMarked
public class PaymentNotShownError {
    private final String mErrorMessage;
    private final @NotShownReason int mNotShownReason;
    private final boolean mIsOffTheRecord;

    /**
     * Creates an instance with the error details for non-off-the-record mode.
     *
     * @param errorMessage The error message for informing the web developer.
     * @param notShownReason The reason the UI was not shown.
     */
    /* package */ PaymentNotShownError(String errorMessage, @NotShownReason int notShownReason) {
        this(errorMessage, notShownReason, /* isOffTheRecord= */ false);
    }

    /**
     * Creates an instance with the error details.
     *
     * @param errorMessage The error message for informing the web developer.
     * @param notShownReason The reason the UI was not shown.
     * @param isOffTheRecord Whether the browser is in off-the-record mode.
     */
    /* package */ PaymentNotShownError(
            String errorMessage, @NotShownReason int notShownReason, boolean isOffTheRecord) {
        assert notShownReason >= 0 && notShownReason < NotShownReason.MAX;
        mErrorMessage = errorMessage;
        mNotShownReason = notShownReason;
        mIsOffTheRecord = isOffTheRecord;
    }

    /** @return The error message for informing the web developer. */
    public String getErrorMessage() {
        return mErrorMessage;
    }

    /**
     * @return The reason of the error, used by the renderer, defined in {@link PaymentErrorReason}.
     */
    public int getPaymentErrorReason() {
        switch (mNotShownReason) {
            case NotShownReason.ALREADY_SHOWING:
                return PaymentErrorReason.ALREADY_SHOWING;
            case NotShownReason.USER_ACTIVATION_REQUIRED:
                return PaymentErrorReason.USER_ACTIVATION_REQUIRED;
            case NotShownReason.BACKGROUND_TAB:
            case NotShownReason.USER_CANCEL:
                return PaymentErrorReason.USER_CANCEL;
            case NotShownReason.NO_SUPPORTED_PAYMENT_METHOD:
                return mIsOffTheRecord
                        ? PaymentErrorReason.USER_CANCEL
                        : PaymentErrorReason.NOT_SUPPORTED;
            default:
                assert false : "Unexpected not shown reason: " + mNotShownReason;
                return PaymentErrorReason.UNKNOWN;
        }
    }

    /**
     * @return The reason the UI was not shown, used for metrics.
     */
    public @NotShownReason int getNotShownReason() {
        return mNotShownReason;
    }
}
