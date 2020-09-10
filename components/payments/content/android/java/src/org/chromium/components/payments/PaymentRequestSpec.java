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
import java.util.ArrayList;
import java.util.Arrays;
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
    // TODO(crbug.com/1124917): these parameters duplicate with those in payment_request_spec from
    // the blink side. We need to de-dup them.
    private final Map<String, PaymentMethodData> mMethodData;
    private final String mId;
    private Map<String, PaymentDetailsModifier> mModifiers = new ArrayMap<>();
    private List<PaymentShippingOption> mRawShippingOptions;
    private List<PaymentItem> mRawLineItems;
    private PaymentItem mRawTotal;
    private long mNativePointer;

    /**
     * Creates a valid instance of PaymentRequestSpec.
     * @param details The payment details, e.g., the total amount.
     * @param options The payment options, e.g., whether shipping is requested.
     * @param methodData The map of supported payment method identifiers and corresponding payment
     * @param appLocale The current application locale.
     * @return The created instance if valid; null otherwise.
     */
    @Nullable
    public static PaymentRequestSpec createAndValidate(PaymentDetails details,
            PaymentOptions options, Map<String, PaymentMethodData> methodData, String appLocale) {
        PaymentRequestSpec spec = new PaymentRequestSpec(details.id, methodData);
        if (!spec.parseAndValidateDetails(details)) return null;
        spec.createNative(details, options, appLocale);
        return spec;
    }

    private PaymentRequestSpec(String id, Map<String, PaymentMethodData> methodData) {
        mId = id;
        mMethodData = methodData;
    }

    private void createNative(PaymentDetails details, PaymentOptions options, String appLocale) {
        assert mNativePointer == 0;
        mNativePointer =
                PaymentRequestSpecJni.get().create(options.serialize(), details.serialize(),
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

    /**
     * The raw shipping options, as it was received from the website. This data is passed to the
     * payment app when the app is responsible for handling shipping address. This value is still
     * available after the instance is destroyed.
     */
    public List<PaymentShippingOption> getRawShippingOptions() {
        return mRawShippingOptions;
    }

    /**
     * The raw items in the shopping cart, as they were received from the website. This data is
     * passed to the payment app. This value is still available after the instance is destroyed.
     */
    public List<PaymentItem> getRawLineItems() {
        return mRawLineItems;
    }

    /**
     * The raw total amount being charged, as it was received from the website. This data is passed
     * to the payment app. This value is still available after the instance is destroyed.
     */
    public PaymentItem getRawTotal() {
        return mRawTotal;
    }

    /**
     * Sets the total, display line items, and shipping options based on input and returns the
     * status boolean. That status is true for valid data, false for invalid data. If the input is
     * invalid, disconnects from the client. Both raw and UI versions of data are updated.
     *
     * @param details The total, line items, and shipping options to parse, validate, and save in
     *                member variables.
     * @return True if the data is valid. False if the data is invalid.
     */
    public boolean parseAndValidateDetails(PaymentDetails details) {
        if (!PaymentValidator.validatePaymentDetails(details)) return false;

        if (details.total != null) {
            mRawTotal = details.total;
        }

        if (mRawLineItems == null || details.displayItems != null) {
            mRawLineItems = Collections.unmodifiableList(details.displayItems != null
                            ? Arrays.asList(details.displayItems)
                            : new ArrayList<>());
        }

        if (details.modifiers != null) {
            if (details.modifiers.length == 0) mModifiers.clear();

            for (int i = 0; i < details.modifiers.length; i++) {
                PaymentDetailsModifier modifier = details.modifiers[i];
                String method = modifier.methodData.supportedMethod;
                mModifiers.put(method, modifier);
            }
        }

        if (details.shippingOptions != null) {
            mRawShippingOptions =
                    Collections.unmodifiableList(Arrays.asList(details.shippingOptions));
        } else if (mRawShippingOptions == null) {
            mRawShippingOptions = Collections.unmodifiableList(new ArrayList<>());
        }

        assert mRawTotal != null;
        assert mRawLineItems != null;

        return true;
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
