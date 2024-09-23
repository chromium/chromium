// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.pm.PackageInfo;
import android.content.pm.Signature;
import android.os.Bundle;
import android.os.RemoteException;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentHandlerMethodData;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentRequestDetailsUpdate;

import java.util.Arrays;

/**
 * Helper class used by android payment app to notify the browser that the user has selected a
 * different payment instrument, shipping option, or shipping address inside native app.
 */
public class PaymentDetailsUpdateServiceHelper {
    private static final String TAG = "PaymentDetailsUpdate";

    @Nullable private IPaymentDetailsUpdateServiceCallback mCallback;
    @Nullable private PaymentRequestUpdateEventListener mListener;
    @Nullable private PackageInfo mInvokedAppPackageInfo;
    @Nullable private PackageManagerDelegate mPackageManagerDelegate;

    // Singleton instance.
    private static PaymentDetailsUpdateServiceHelper sInstance;

    private PaymentDetailsUpdateServiceHelper() {}

    /**
     * Returns the singleton instance, lazily creating one if needed. The instance is only useful
     * after its listener is set which happens when a native android app gets invoked.
     *
     * @return The singleton instance.
     */
    public static PaymentDetailsUpdateServiceHelper getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new PaymentDetailsUpdateServiceHelper();
        return sInstance;
    }

    /**
     * Initializes the service helper, called when an AndroidPaymentApp is invoked.
     * @param packageManagerDelegate The package manager used used to authorize the connecting app.
     * @param invokedAppPackageName The package name of the invoked payment app, used to authorize
     *         the connecting app.
     * @param listener The listener for payment method, shipping address, and shipping option
     *         changes.
     */
    public void initialize(
            PackageManagerDelegate packageManagerDelegate,
            String invokedAppPackageName,
            PaymentRequestUpdateEventListener listener) {
        ThreadUtils.assertOnUiThread();
        assert mListener == null;
        mListener = listener;
        mPackageManagerDelegate = packageManagerDelegate;
        mInvokedAppPackageInfo =
                mPackageManagerDelegate.getPackageInfoWithSignatures(invokedAppPackageName);
    }

    /**
     * Called to notify the merchant that the user has selected a different payment method.
     *
     * @param paymentHandlerMethodData The data containing the selected payment method's name and
     *     optional stringified details.
     * @param callback The callback used to notify the invoked app about updated payment details.
     */
    public void changePaymentMethod(
            Bundle paymentHandlerMethodData, IPaymentDetailsUpdateServiceCallback callback) {
        ThreadUtils.assertOnUiThread();
        RecordHistogram.recordBooleanHistogram(
                "PaymentRequest.PaymentDetailsUpdateService.ChangePaymentMethod", true);
        if (paymentHandlerMethodData == null) {
            runCallbackWithError(ErrorStrings.METHOD_DATA_REQUIRED, callback);
            return;
        }
        String methodName =
                paymentHandlerMethodData.getString(PaymentHandlerMethodData.EXTRA_METHOD_NAME);
        if (TextUtils.isEmpty(methodName)) {
            runCallbackWithError(ErrorStrings.METHOD_NAME_REQUIRED, callback);
            return;
        }

        String stringifiedDetails =
                paymentHandlerMethodData.getString(
                        PaymentHandlerMethodData.EXTRA_STRINGIFIED_DETAILS,
                        /* defaultValue= */ "{}");
        if (isWaitingForPaymentDetailsUpdate()
                || mListener == null
                || !mListener.changePaymentMethodFromInvokedApp(methodName, stringifiedDetails)) {
            runCallbackWithError(ErrorStrings.INVALID_STATE, callback);
            return;
        }
        mCallback = callback;
    }

    /**
     * Called to notify the merchant that the user has selected a different shipping option.
     *
     * @param shippingOptionId The identifier of the selected shipping option.
     * @param callback The callback used to notify the invoked app about updated payment details.
     */
    public void changeShippingOption(
            String shippingOptionId, IPaymentDetailsUpdateServiceCallback callback) {
        ThreadUtils.assertOnUiThread();
        RecordHistogram.recordBooleanHistogram(
                "PaymentRequest.PaymentDetailsUpdateService.ChangeShippingOption", true);
        if (TextUtils.isEmpty(shippingOptionId)) {
            runCallbackWithError(ErrorStrings.SHIPPING_OPTION_ID_REQUIRED, callback);
            return;
        }

        if (isWaitingForPaymentDetailsUpdate()
                || mListener == null
                || !mListener.changeShippingOptionFromInvokedApp(shippingOptionId)) {
            runCallbackWithError(ErrorStrings.INVALID_STATE, callback);
            return;
        }
        mCallback = callback;
    }

    /**
     * Called to notify the merchant that the user has selected a different shipping address.
     *
     * @param shippingAddress The selected shipping address
     * @param callback The callback used to notify the invoked app about updated payment details.
     */
    public void changeShippingAddress(
            Bundle shippingAddress, IPaymentDetailsUpdateServiceCallback callback) {
        ThreadUtils.assertOnUiThread();
        RecordHistogram.recordBooleanHistogram(
                "PaymentRequest.PaymentDetailsUpdateService.ChangeShippingAddress", true);
        if (shippingAddress == null || shippingAddress.isEmpty()) {
            runCallbackWithError(ErrorStrings.SHIPPING_ADDRESS_INVALID, callback);
            return;
        }

        Address address = Address.createFromBundle(shippingAddress);
        if (!address.isValid()) {
            runCallbackWithError(ErrorStrings.SHIPPING_ADDRESS_INVALID, callback);
            return;
        }

        if (isWaitingForPaymentDetailsUpdate()
                || mListener == null
                || !mListener.changeShippingAddressFromInvokedApp(
                        PaymentAddressTypeConverter.convertAddressToMojoPaymentAddress(address))) {
            runCallbackWithError(ErrorStrings.INVALID_STATE, callback);
            return;
        }
        mCallback = callback;
    }

    /** Resets the singleton instance. */
    public void reset() {
        ThreadUtils.assertOnUiThread();
        sInstance = null;
    }

    /**
     * Checks whether any payment method, shipping address or shipping option change event is
     * ongoing.
     * @return True after invoked payment app has bound PaymentDetaialsUpdateService and called
     *         changePaymentMethod, changeShippingAddress, or changeShippingOption and before the
     *         merchant replies with either updateWith() or onPaymentDetailsNotUpdated().
     */
    public boolean isWaitingForPaymentDetailsUpdate() {
        ThreadUtils.assertOnUiThread();
        return mCallback != null;
    }

    /**
     * Notifies the invoked app about merchant's response to the change event.
     * @param response - Modified payment request details to be sent to the invoked app.
     */
    public void updateWith(PaymentRequestDetailsUpdate response) {
        ThreadUtils.assertOnUiThread();
        if (mCallback == null) return;
        try {
            mCallback.updateWith(response.asBundle());
        } catch (RemoteException e) {
            Log.e(TAG, "Error calling updateWith", e);
        } finally {
            mCallback = null;
        }
    }

    /**
     * Notfies the invoked app that the merchant has not updated any of the payment request details
     * in response to a change event.
     */
    public void onPaymentDetailsNotUpdated() {
        ThreadUtils.assertOnUiThread();
        if (mCallback == null) return;
        try {
            mCallback.paymentDetailsNotUpdated();
        } catch (RemoteException e) {
            Log.e(TAG, "Error calling paymentDetailsNotUpdated", e);
        } finally {
            mCallback = null;
        }
    }

    /**
     * @param callerUid The Uid of the service requester.
     * @return True when the service requester's package name and signature are the same as the
     *         invoked payment app's.
     */
    public boolean isCallerAuthorized(int callerUid) {
        ThreadUtils.assertOnUiThread();
        if (mPackageManagerDelegate == null) {
            Log.e(TAG, ErrorStrings.UNATHORIZED_SERVICE_REQUEST);
            return false;
        }
        PackageInfo callerPackageInfo =
                mPackageManagerDelegate.getPackageInfoWithSignatures(callerUid);
        if (mInvokedAppPackageInfo == null
                || callerPackageInfo == null
                || !mInvokedAppPackageInfo.packageName.equals(callerPackageInfo.packageName)) {
            Log.e(TAG, ErrorStrings.UNATHORIZED_SERVICE_REQUEST);
            return false;
        }

        // TODO(crbug.com/40694276): signatures field is deprecated in API level 28.
        Signature[] callerSignatures = callerPackageInfo.signatures;
        Signature[] invokedAppSignatures = mInvokedAppPackageInfo.signatures;

        boolean result = Arrays.equals(callerSignatures, invokedAppSignatures);
        if (!result) Log.e(TAG, ErrorStrings.UNATHORIZED_SERVICE_REQUEST);
        return result;
    }

    private void runCallbackWithError(
            String errorMessage, IPaymentDetailsUpdateServiceCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (callback == null) return;
        // Only populate the error field.
        Bundle blankUpdatedPaymentDetails = new Bundle();
        blankUpdatedPaymentDetails.putString(
                PaymentRequestDetailsUpdate.EXTRA_ERROR_MESSAGE, errorMessage);
        try {
            callback.updateWith(blankUpdatedPaymentDetails);
        } catch (RemoteException e) {
            Log.e(TAG, "Error calling updateWith", e);
        }
    }
}
