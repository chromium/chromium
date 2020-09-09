// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.nio.ByteBuffer;
import java.util.List;
import java.util.Map;

/**
 * Container for information received from the renderer that invoked the Payment Request API. Owns
 * an instance of native payment_request_spec.cc, so the destroy() method has to be called to free
 * the native pointer.
 */
@JNINamespace("payments::android")
public class PaymentRequestSpec {
    // TODO(crbug.com/1124917): these parameters duplicate with those in payment_request_spec from
    // the blink side. We need to de-dup them.
    private Map<String, PaymentDetailsModifier> mModifiers = new ArrayMap<>();
    private String mId;
    private List<PaymentShippingOption> mRawShippingOptions;
    private List<PaymentItem> mRawLineItems;
    private PaymentItem mRawTotal;
    private final PaymentOptions mOptions;
    private final Map<String, PaymentMethodData> mMethodData;
    private long mNativePointer;

    /**
     * Creates an instance to store the information received from the renderer that invoked the
     * Payment Request API.
     * @param options The payment options, e.g., whether shipping is requested.
     * @param methodData The map of supported payment method identifiers and corresponding payment
     * method specific data.
     */
    public PaymentRequestSpec(PaymentOptions options, Map<String, PaymentMethodData> methodData) {
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
        mNativePointer =
                PaymentRequestSpecJni.get().create(mOptions.serialize(), details.serialize(),
                        MojoStructCollection.serialize(mMethodData.values()), appLocale);
    }

    /** @return Whether destroy() has been called. */
    public boolean isDestroyed() {
        return mNativePointer == 0;
    }

    /**
     * @return The map of supported payment method identifiers and corresponding payment
     * method specific data. This value is still available after the instance is destroyed.
     */
    public Map<String, PaymentMethodData> getMethodData() {
        return mMethodData;
    }

    /**
     * A mapping from method names to modifiers, which include modified totals and additional line
     * items. Used to display modified totals for each payment apps, modified total in order
     * summary, and additional line items in order summary. This value is still available after the
     * instance is destroyed.
     */
    public Map<String, PaymentDetailsModifier> getModifiers() {
        return mModifiers;
    }

    /**
     * @return The id of the request, found in PaymentDetails. This value is still available after
     *         the instance is destroyed.
     */
    public String getId() {
        return mId;
    }

    /** Set the id found from PaymentDetails. */
    public void setId(String id) {
        mId = id;
    }

    /**
     * The raw shipping options, as it was received from the website. This data is passed to the
     * payment app when the app is responsible for handling shipping address. This value is still
     * available after the instance is destroyed.
     */
    public List<PaymentShippingOption> getRawShippingOptions() {
        return mRawShippingOptions;
    }

    /** Set the raw shipping options found from PaymentDetails. */
    public void setRawShippingOptions(List<PaymentShippingOption> rawShippingOptions) {
        mRawShippingOptions = rawShippingOptions;
    }

    /**
     * The raw items in the shopping cart, as they were received from the website. This data is
     * passed to the payment app. This value is still available after the instance is destroyed.
     */
    public List<PaymentItem> getRawLineItems() {
        return mRawLineItems;
    }

    /** Set the raw payment items found in PaymentDetails. */
    public void setRawLineItems(List<PaymentItem> rawLineItems) {
        mRawLineItems = rawLineItems;
    }

    /**
     * The raw total amount being charged, as it was received from the website. This data is passed
     * to the payment app. This value is still available after the instance is destroyed.
     */
    public PaymentItem getRawTotal() {
        return mRawTotal;
    }

    /** Set the total property found from PaymentDetails. */
    public void setRawTotal(PaymentItem rawTotal) {
        mRawTotal = rawTotal;
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

    /**
     * Recomputes spec based on details. This cannot be used after the instance is destroyed.
     */
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
