// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;

import java.nio.ByteBuffer;

/**
 * Handles the communication from the payment handler renderer process to the merchant renderer
 * process.
 */
@JNINamespace("payments::android")
public class PaymentHandlerHost {
    /**
     * The interface to be implemented by the object that can communicate to the merchant renderer
     * process.
     */
    public interface PaymentHandlerHostDelegate {
        /**
         * Notifies the merchant that the payment method has changed within a payment handler. The
         * merchant may recalculate the total based on the changed billing address, for example.
         * @param methodName      The payment method identifier.
         * @param stringifiedData The stringified method-specific data.
         * @return "False" if not in a valid state.
         */
        @CalledByNative("PaymentHandlerHostDelegate")
        boolean changePaymentMethodFromPaymentHandler(String methodName, String stringifiedData);
    }

    /** Pointer to the native bridge. This Java object owns the native bridge. */
    private long mNativePointer;

    /**
     * Instantiates the native bridge to the payment handler host. This Java object owns the native
     * bridge. The caller must call destroy() when finished using this Java object.
     * @param webContents The web contents in the same browser context as the payment handler. Used
     *                    for logging in developer tools.
     * @param delegate    The object that can communicate to the merchant renderer process.
     */
    public PaymentHandlerHost(WebContents webContents, PaymentHandlerHostDelegate delegate) {
        mNativePointer = PaymentHandlerHostJni.get().init(webContents, delegate);
    }

    /**
     * Checks whether the payment method change event is ongoing.
     * @return True after payment handler called changePaymentMethod() and before the merchant
     * replies with either updateWith() or noUpdatedPaymentDetails().
     */
    public boolean isChangingPaymentMethod() {
        return PaymentHandlerHostJni.get().isChangingPaymentMethod(mNativePointer);
    }

    /**
     * Returns the pointer to the native payment handler host object. The native bridge owns this
     * object.
     * @return The pointer to the native payments::PaymentHandlerHost (not the native bridge
     *         payments::android::PaymentHandlerHost).
     */
    public long getNativePaymentHandlerHost() {
        return PaymentHandlerHostJni.get().getNativePaymentHandlerHost(mNativePointer);
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
     * Notifies the payment handler that the merchant ignored the the payment-method-change event.
     */
    public void noUpdatedPaymentDetails() {
        PaymentHandlerHostJni.get().noUpdatedPaymentDetails(mNativePointer);
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
         * @param delegate    The object that can communicate to the merchant renderer process.
         * @return The pointer to the native payment handler host bridge.
         */
        long init(WebContents webContents, PaymentHandlerHostDelegate delegate);

        /**
         * Checks whether the payment method change is currently in progress.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         */
        boolean isChangingPaymentMethod(long nativePaymentHandlerHost);

        /**
         * Returns the native pointer to the payment handler host (not the bridge). The native
         * bridge owns the returned pointer.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         * @return The pointer to the native payment handler host.
         */
        long getNativePaymentHandlerHost(long nativePaymentHandlerHost);

        /**
         * Notifies the payment handler that the merchant has updated the payment details.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         * @param responseBuffer The serialized payment method change response from the merchant.
         */
        void updateWith(long nativePaymentHandlerHost, ByteBuffer responseBuffer);

        /**
         * Notifies the payment handler that the merchant ignored the payment method change event.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         */
        void noUpdatedPaymentDetails(long nativePaymentHandlerHost);

        /**
         * Destroys the native payment handler host bridge.
         * @param nativePaymentHandlerHost The pointer to the native payment handler host bridge.
         */
        void destroy(long nativePaymentHandlerHost);
    }
}
