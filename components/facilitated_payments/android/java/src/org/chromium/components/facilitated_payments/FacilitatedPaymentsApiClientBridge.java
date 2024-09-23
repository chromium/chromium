// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.content_public.browser.RenderFrameHost;

/** Native bridge for facilitated payment APIs, such as PIX. */
@JNINamespace("payments::facilitated")
public class FacilitatedPaymentsApiClientBridge implements FacilitatedPaymentsApiClient.Delegate {
    private final FacilitatedPaymentsApiClient mApiClient;
    private long mNativeFacilitatedPaymentsApiClientAndroid;

    /**
     * Constructs an instance of the bridge for invoking the facilitated payments API.
     *
     * @param nativeFacilitatedPaymentsApiClientAndroid The pointer to the C++ object that receives
     *     responses from the API.
     * @param renderFrameHost The RenderFrameHost used for retrieving the Android context.
     */
    @CalledByNative
    public FacilitatedPaymentsApiClientBridge(
            long nativeFacilitatedPaymentsApiClientAndroid, RenderFrameHost renderFrameHost) {
        mNativeFacilitatedPaymentsApiClientAndroid = nativeFacilitatedPaymentsApiClientAndroid;
        mApiClient = FacilitatedPaymentsApiClient.create(renderFrameHost, /* delegate= */ this);
        assert mApiClient != null;
    }

    /**
     * Resets the pointer to the C++ object, so it no longer receives any responses from the API.
     * The C++ object can be deleted afterwards.
     */
    @CalledByNative
    public void resetNativePointer() {
        mNativeFacilitatedPaymentsApiClientAndroid = 0;
    }

    /**
     * Checks whether facilitated payments API is available to use. The result is received back in
     * the onIsAvailable(boolean) method. If the boolean is false, then no FOP (form of payment)
     * selector should be shown.
     */
    @CalledByNative
    public void isAvailable() {
        mApiClient.isAvailable();
    }

    /**
     * Retrieves the client token for initiating payment. The client token will be received back in
     * the onGetClientToken(byte[]) method. If the client token is null or empty, then payment
     * should not be initiated.
     */
    @CalledByNative
    public void getClientToken() {
        mApiClient.getClientToken();
    }

    /**
     * Initiates the payment flow UI by invoking the payment manager with the action token. The
     * result is received back in the onPurchaseActionResultEnum(PurchaseActionResult) method.
     *
     * @param primaryAccount User's signed in account.
     * @param actionToken An opaque token used for invoking the purchase action.
     */
    @CalledByNative
    public void invokePurchaseAction(CoreAccountInfo primaryAccount, byte[] actionToken) {
        mApiClient.invokePurchaseAction(primaryAccount, actionToken);
    }

    // FacilitatedPaymentsApiClient.Delegate implementation:
    @Override
    public void onIsAvailable(boolean isAvailable) {
        if (mNativeFacilitatedPaymentsApiClientAndroid == 0) return;
        FacilitatedPaymentsApiClientBridgeJni.get()
                .onIsAvailable(mNativeFacilitatedPaymentsApiClientAndroid, isAvailable);
    }

    // FacilitatedPaymentsApiClient.Delegate implementation:
    @Override
    public void onGetClientToken(byte[] clientToken) {
        if (mNativeFacilitatedPaymentsApiClientAndroid == 0) return;
        FacilitatedPaymentsApiClientBridgeJni.get()
                .onGetClientToken(mNativeFacilitatedPaymentsApiClientAndroid, clientToken);
    }

    // FacilitatedPaymentsApiClient.Delegate implementation:
    @Override
    public void onPurchaseActionResultEnum(@PurchaseActionResult int purchaseActionResult) {
        if (mNativeFacilitatedPaymentsApiClientAndroid == 0) return;
        FacilitatedPaymentsApiClientBridgeJni.get().onPurchaseActionResultEnum(
                mNativeFacilitatedPaymentsApiClientAndroid, purchaseActionResult);
    }

    @NativeMethods
    interface Natives {
        void onIsAvailable(long nativeFacilitatedPaymentsApiClientAndroid, boolean isAvailable);

        void onGetClientToken(long nativeFacilitatedPaymentsApiClientAndroid, byte[] clientToken);

        void onPurchaseActionResultEnum(long nativeFacilitatedPaymentsApiClientAndroid,
                @PurchaseActionResult int purchaseActionResult);
    }
}
