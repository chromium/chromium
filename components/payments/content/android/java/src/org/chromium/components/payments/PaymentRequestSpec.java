// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Container for information received from the renderer that invoked the Payment Request API. Owns
 * an instance of native payment_request_spec.cc, so the destroy() method has to be called to free
 * the native pointer.
 */
@JNINamespace("payments::android")
public class PaymentRequestSpec {
    private long mNativePointer;

    /**
     * Stores the information received from the renderer that invoked the Payment Request API.
     * Creates an instance of native payment_request_spec.cc with the given parameters.
     * @param options The payment options, e.g., whether shipping is requested.
     * @param details The payment details, e.g., the total amount.
     * @param methodData The list of supported payment method identifiers and corresponding payment
     * method specific data.
     * @param appLocale The current application locale.
     */
    public PaymentRequestSpec(
            PaymentOptions options,
            PaymentDetails details,
            Collection<PaymentMethodData> methodData,
            String appLocale) {
        mNativePointer =
                PaymentRequestSpecJni.get()
                        .create(
                                options.serialize(),
                                details.serialize(),
                                MojoStructCollection.serialize(methodData),
                                appLocale);
    }

    /** @return Whether destroy() has been called. */
    public boolean isDestroyed() {
        return mNativePointer == 0;
    }

    /**
     * @return The payment options specified by the merchant. This method cannot be used after the
     *         instance is destroyed.
     */
    public PaymentOptions getPaymentOptions() {
        return PaymentOptions.deserialize(
                ByteBuffer.wrap(PaymentRequestSpecJni.get().getPaymentOptions(mNativePointer)));
    }

    /**
     * @return The map of supported payment method identifiers and corresponding payment
     * method specific data. This method cannot be used after the instance is destroyed.
     */
    public Map<String, PaymentMethodData> getMethodData() {
        Map<String, PaymentMethodData> methodDataMap = new ArrayMap<>();
        byte[][] methodDataByteArrays = PaymentRequestSpecJni.get().getMethodData(mNativePointer);
        for (int i = 0; i < methodDataByteArrays.length; i++) {
            PaymentMethodData methodData =
                    PaymentMethodData.deserialize(ByteBuffer.wrap(methodDataByteArrays[i]));
            String method = methodData.supportedMethod;
            methodDataMap.put(method, methodData);
        }
        return methodDataMap;
    }

    /**
     * A mapping from method names to modifiers, which include modified totals and additional line
     * items. Used to display modified totals for each payment apps, modified total in order
     * summary, and additional line items in order summary. This method cannot be used after the
     * instance is destroyed.
     */
    public Map<String, PaymentDetailsModifier> getModifiers() {
        Map<String, PaymentDetailsModifier> modifiers = new ArrayMap<>();
        PaymentDetails details = getPaymentDetails();
        if (details.modifiers != null) {
            for (int i = 0; i < details.modifiers.length; i++) {
                PaymentDetailsModifier modifier = details.modifiers[i];
                String method = modifier.methodData.supportedMethod;
                modifiers.put(method, modifier);
            }
        }
        return modifiers;
    }

    /**
     * @return The id of the request, found in PaymentDetails. This method cannot be used after the
     *         instance is destroyed.
     */
    public String getId() {
        return getPaymentDetails().id;
    }

    /**
     * The raw shipping options, as it was received from the website. This data is passed to the
     * payment app when the app is responsible for handling shipping address. This method cannot be
     * used after the instance is destroyed.
     */
    public List<PaymentShippingOption> getRawShippingOptions() {
        PaymentDetails details = getPaymentDetails();
        return Collections.unmodifiableList(
                details.shippingOptions != null
                        ? Arrays.asList(details.shippingOptions)
                        : new ArrayList<>());
    }

    /**
     * The raw items in the shopping cart, as they were received from the website. This data is
     * passed to the payment app. This method cannot be used after the instance is destroyed.
     */
    public List<PaymentItem> getRawLineItems() {
        PaymentDetails details = getPaymentDetails();
        return Collections.unmodifiableList(
                details.displayItems != null
                        ? Arrays.asList(details.displayItems)
                        : new ArrayList<>());
    }

    /**
     * The raw total amount being charged, as it was received from the website. This data is passed
     * to the payment app. This method cannot be used after the instance is destroyed.
     */
    public PaymentItem getRawTotal() {
        return getPaymentDetails().total;
    }

    /**
     * @return The payment details specified in the payment request. This method cannot be used
     *         after the instance is destroyed.
     */
    public PaymentDetails getPaymentDetails() {
        return PaymentDetails.deserialize(
                ByteBuffer.wrap(PaymentRequestSpecJni.get().getPaymentDetails(mNativePointer)));
    }

    /**
     * Called when the renderer updates the payment details in response to, e.g., new shipping
     * address. This cannot be used after the instance is destroyed.
     * @param details The updated payment details, e.g., the updated total amount.
     */
    public void updateWith(PaymentDetails details) {
        PaymentRequestSpecJni.get().updateWith(mNativePointer, details.serialize());
    }

    /**
     * Called when merchant retries a failed payment. This cannot be used after the instance is
     * destroyed.
     * @param validationErrors The information about the fields that failed the validation.
     */
    public void retry(PaymentValidationErrors validationErrors) {
        PaymentRequestSpecJni.get().retry(mNativePointer, validationErrors.serialize());
    }

    /** Recomputes spec based on details. This cannot be used after the instance is destroyed. */
    public void recomputeSpecForDetails() {
        PaymentRequestSpecJni.get().recomputeSpecForDetails(mNativePointer);
    }

    /**
     * Returns the selected shipping option error. This cannot be used after the instance is
     * destroyed.
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

    @CalledByNative
    private long getNativePointer() {
        return mNativePointer;
    }

    /** Whether the secure-payment-confirmation method is requested. */
    public boolean isSecurePaymentConfirmationRequested() {
        return PaymentRequestSpecJni.get().isSecurePaymentConfirmationRequested(mNativePointer);
    }

    @NativeMethods
    /* package */ interface Natives {
        long create(
                ByteBuffer optionsByteBuffer,
                ByteBuffer detailsByteBuffer,
                ByteBuffer[] methodDataByteBuffers,
                String appLocale);

        void updateWith(long nativePaymentRequestSpec, ByteBuffer detailsByteBuffer);

        void retry(long nativePaymentRequestSpec, ByteBuffer validationErrorsByteBuffer);

        void recomputeSpecForDetails(long nativePaymentRequestSpec);

        String selectedShippingOptionError(long nativePaymentRequestSpec);

        void destroy(long nativePaymentRequestSpec);

        byte[] getPaymentDetails(long nativePaymentRequestSpec);

        byte[][] getMethodData(long nativePaymentRequestSpec);

        byte[] getPaymentOptions(long nativePaymentRequestSpec);

        boolean isSecurePaymentConfirmationRequested(long nativePaymentRequestSpec);
    }
}
