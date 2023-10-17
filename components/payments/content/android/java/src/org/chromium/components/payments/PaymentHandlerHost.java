// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;

import java.nio.ByteBuffer;

/**
 * Handles the communication from the payment handler renderer process to the merchant renderer
 * process.
 */
@JNINamespace("payments::android")
public class PaymentHandlerHost {
    /** Pointer to the native bridge. This Java object owns the native bridge. */
    private long mNativePointer;

    /**
     * Instantiates the native bridge to the payment handler host. This Java object owns the native
     * bridge. The caller must call destroy() when finished using this Java object.
     * @param webContents The web contents in the same browser context as the payment handler. Used
     *                    for logging in developer tools.
     * @param listener    The object that can communicate to the merchant renderer process.
     */
    public PaymentHandlerHost(WebContents webContents, PaymentRequestUpdateEventListener listener) {
        mNativePointer = PaymentHandlerHostJni.get().init(webContents, listener);
    }

    /**
     * Checks whether any payment method, shipping address or shipping option change event is
     * ongoing.
     * @return True after payment handler called changePaymentMethod(), changeShippingAddress(), or
     *         changeShippingOption() and before the merchant replies with either updateWith() or
     *         onPaymentDetailsNotUpdated().
     */
    public boolean isWaitingForPaymentDetailsUpdate() {
        return PaymentHandlerHostJni.get().isWaitingForPaymentDetailsUpdate(mNativePointer);
    }

    /**
     * Returns the pointer to the native bridge. The Java object owns this bridge.
     * @return The pointer to the native bridge payments::android::PaymentHandlerHost (not the
     *         cross-platform payment handler host payments::PaymentHandlerHost).
     */
    @CalledByNative
    public long getNativeBridge() {
        return mNativePointer;
    }

    /**
     * Notifies the payment handler that the merchant has updated the payment details in response to
     * the payment-method-change event or shipping-[address|option]-change events.
     * @param response The payment request details update response. Should not be null.
     */
    public void updateWith(PaymentRequestDetailsUpdate response) {
        assert response != null;
        PaymentHandlerHostJni.get().updateWith(mNativePointer, response.serialize());
    }

    /**
     * Notifies the payment handler that the merchant ignored the the payment-method,
     * shipping-address, or shipping-option change event.
     */
    public void onPaymentDetailsNotUpdated() {
        PaymentHandlerHostJni.get().onPaymentDetailsNotUpdated(mNativePointer);
    }

    /** Destroys the native bridge. This object shouldn't be used afterwards. */
    public void destroy() {
        PaymentHandlerHostJni.get().destroy(mNativePointer);
        mNativePointer = 0;
    }

    /**
     * The interface implemented by the automatically generated JNI bindings class
     * PaymentHandlerHostJni.
     */
    @NativeMethods
    /* package */ interface Natives {
        /**
         * Initializes the native object. The Java caller owns the returned native object and must
         * call destroy(nativePaymentHandlerHost) when done.
         * @param webContents The web contents in the same browser context as the payment handler.
         *                    Used for logging in developer tools.
         * @param listener    The object that can communicate to the merchant renderer process.
         * @return The pointer to the native payment handler host bridge.
         */
        long init(WebContents webContents, PaymentRequestUpdateEventListener listener);

        /**
         * Checks whether any payment method, shipping address, or shipping option change is
         * currently in progress.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         */
        boolean isWaitingForPaymentDetailsUpdate(long nativePaymentHandlerHost);

        /**
         * Notifies the payment handler that the merchant has updated the payment details.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         * @param responseBuffer The serialized payment method change response from the merchant.
         */
        void updateWith(long nativePaymentHandlerHost, ByteBuffer responseBuffer);

        /**
         * Notifies the payment handler that the merchant ignored the payment method, shipping
         * address, or shipping option change event.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         */
        void onPaymentDetailsNotUpdated(long nativePaymentHandlerHost);

        /**
         * Destroys the native payment handler host bridge.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         */
        void destroy(long nativePaymentHandlerHost);
    }
}
