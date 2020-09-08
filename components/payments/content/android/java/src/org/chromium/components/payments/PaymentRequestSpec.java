// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.nio.ByteBuffer;
import java.util.Collection;

/**
 * Container for information received from the renderer that invoked the Payment Request API. Owns
 * an instance of native payment_request_spec.cc, so the destroy() method has to be called to free
 * the native pointer.
 */
@JNINamespace("payments::android")
public class PaymentRequestSpec {
    private final PaymentOptions mOptions;
    private final Collection<PaymentMethodData> mMethodData;
    private long mNativePointer;

    /**
     * Creates an instance to store the information received from the renderer that invoked the
     * Payment Request API.
     * @param options The payment options, e.g., whether shipping is requested.
     * @param methodData The list of supported payment method identifiers and corresponding payment
     * method specific data.
     */
    public PaymentRequestSpec(PaymentOptions options, Collection<PaymentMethodData> methodData) {
        mOptions = options;
        mMethodData = methodData;
    }

    /**
     * Creates an instance of native payment_request_spec.cc with the existing and the given
     * parameters.
     * @param details The payment details, e.g., the total amount.
     * @param appLocale The current application locale.
     */
    public void createNative(PaymentDetails details, String appLocale) {
        mNativePointer = PaymentRequestSpecJni.get().create(mOptions.serialize(),
                details.serialize(), MojoStructCollection.serialize(mMethodData), appLocale);
    }

    /**
     * Called when the renderer updates the payment details in response to, e.g., new shipping
     * address.
     * @param details The updated payment details, e.g., the updated total amount.
     */
    public void updateWith(PaymentDetails details) {
        PaymentRequestSpecJni.get().updateWith(mNativePointer, details.serialize());
    }

    /**
     * Called when merchant retries a failed payment.
     * @param validationErrors The information about the fields that failed the validation.
     */
    public void retry(PaymentValidationErrors validationErrors) {
        PaymentRequestSpecJni.get().retry(mNativePointer, validationErrors.serialize());
    }

    /**
     * Recomputes spec based on details.
     */
    public void recomputeSpecForDetails() {
        PaymentRequestSpecJni.get().recomputeSpecForDetails(mNativePointer);
    }

    /**
     * Returns the selected shipping option error.
     */
    @Nullable
    public String selectedShippingOptionError() {
        return PaymentRequestSpecJni.get().selectedShippingOptionError(mNativePointer);
    }

    /** Destroys the native pointer. */
    public void destroy() {
        if (mNativePointer == 0) return;
        PaymentRequestSpecJni.get().destroy(mNativePointer);
        mNativePointer = 0;
    }

    /**
     * Destroys native object if owned object is not destroyed.
     *
     * @see java.lang.Object#finalize()
     */
    @Override
    public void finalize() throws Throwable {
        destroy();
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativePointer;
    }

    @NativeMethods
    /* package */ interface Natives {
        long create(ByteBuffer optionsByteBuffer, ByteBuffer detailsByteBuffer,
                ByteBuffer[] methodDataByteBuffers, String appLocale);
        void updateWith(long nativePaymentRequestSpec, ByteBuffer detailsByteBuffer);
        void retry(long nativePaymentRequestSpec, ByteBuffer validationErrorsByteBuffer);
        void recomputeSpecForDetails(long nativePaymentRequestSpec);
        String selectedShippingOptionError(long nativePaymentRequestSpec);
        void destroy(long nativePaymentRequestSpec);
    }
}
