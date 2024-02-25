// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.intent;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.IsReadyToPayService;
import org.chromium.IsReadyToPayServiceCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.payments.PrePurchaseQuery;

/** A helper to query the payment app's IsReadyToPay service. */
public class IsReadyToPayServiceHelper extends IsReadyToPayServiceCallback.Stub
        implements ServiceConnection {
    /** The maximum number of milliseconds to wait for a response from a READY_TO_PAY service. */
    private static final long READY_TO_PAY_TIMEOUT_MS = 400;

    /** The maximum number of milliseconds to wait for a connection to READY_TO_PAY service. */
    private static final long SERVICE_CONNECTION_TIMEOUT_MS = 1000;

    private final Context mContext;

    // This callback can be used only once, set to null after that.
    private ResultHandler mResultHandler;

    private boolean mIsServiceBindingInitiated;
    private boolean mIsReadyToPayQueried;
    private Handler mHandler;
    private Intent mIsReadyToPayIntent;

    /** The callback that returns the result (success or error) to the helper's caller. */
    public interface ResultHandler {
        /**
         * Invoked when the service receives the response.
         * @param isReadyToPay The service response.
         */
        void onIsReadyToPayServiceResponse(boolean isReadyToPay);

        /** Invoked when the service has any error. */
        void onIsReadyToPayServiceError();
    }

    /**
     * Initiate the helper.
     * @param context The application context. Should not be null.
     * @param isReadyToPayIntent The IsReaddyToPay intent created by {@link
     *         WebPaymentIntentHelper#createIsReadyToPayIntent}. Should not be null.
     * @param resultHandler Invoked when the service's result is known. Should not be null.
     */
    public IsReadyToPayServiceHelper(
            Context context, Intent isReadyToPayIntent, ResultHandler resultHandler) {
        assert context != null;
        assert isReadyToPayIntent != null;
        assert resultHandler != null;
        mContext = context;
        mResultHandler = resultHandler;
        mHandler = new Handler();
        mIsReadyToPayIntent = isReadyToPayIntent;
    }

    /**
     * Query the IsReadyToPay service. The result would be returned in the resultHandler callback
     * asynchronously. Note that resultHandler would be invoked only once.
     */
    public void query() {
        try {
            // This method returns "true if the system is in the process of bringing up a
            // service that your client has permission to bind to; false if the system couldn't
            // find the service or if your client doesn't have permission to bind to it. If this
            // value is true, you should later call unbindService(ServiceConnection) to release
            // the connection."
            // https://developer.android.com/reference/android/content/Context.html#bindService(android.content.Intent,%20android.content.ServiceConnection,%20int)
            mIsServiceBindingInitiated =
                    mContext.bindService(
                            mIsReadyToPayIntent,
                            /* serviceConnection= */ this,
                            Context.BIND_AUTO_CREATE);
        } catch (SecurityException e) {
            // Intentionally blank, so mIsServiceBindingInitiated is false.
        }

        if (!mIsServiceBindingInitiated) {
            reportError();
            return;
        }

        mHandler.postDelayed(
                () -> {
                    if (!mIsReadyToPayQueried) reportError();
                },
                SERVICE_CONNECTION_TIMEOUT_MS);
    }

    // ServiceConnection:
    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        // Timeout could cause the null.
        if (mResultHandler == null) return;

        IsReadyToPayService isReadyToPayService = IsReadyToPayService.Stub.asInterface(service);
        if (isReadyToPayService == null) {
            reportError();
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "PaymentRequest.PrePurchaseQuery",
                PrePurchaseQuery.ANDROID_INTENT,
                PrePurchaseQuery.MAX_VALUE);
        mIsReadyToPayQueried = true;
        try {
            isReadyToPayService.isReadyToPay(/* callback= */ this);
        } catch (Throwable e) {
            // Many undocumented exceptions are not caught in the remote Service but passed on
            // to the Service caller, see writeException in Parcel.java.
            reportError();
            return;
        }
        mHandler.postDelayed(this::reportError, READY_TO_PAY_TIMEOUT_MS);
    }

    // "Called when a connection to the Service has been lost. This typically happens
    // when the process hosting the service has crashed or been killed. This does not
    // remove the ServiceConnection itself -- this binding to the service will remain
    // active, and you will receive a call to onServiceConnected(ComponentName, IBinder)
    // when the Service is next running."
    // https://developer.android.com/reference/android/content/ServiceConnection.html#onServiceDisconnected(android.content.ComponentName)
    @Override
    public void onServiceDisconnected(ComponentName name) {
        // Do not wait for the service to restart.
        reportError();
    }

    // IsReadyToPayServiceCallback.Stub:
    @Override
    public void handleIsReadyToPay(boolean isReadyToPay) throws RemoteException {
        if (mResultHandler == null) return;
        mResultHandler.onIsReadyToPayServiceResponse(isReadyToPay);
        mResultHandler = null;
        destroy();
    }

    private void reportError() {
        if (mResultHandler == null) return;
        mResultHandler.onIsReadyToPayServiceError();
        mResultHandler = null;
        destroy();
    }

    /** Clean up the resources that this helper has created. */
    private void destroy() {
        if (mIsServiceBindingInitiated) {
            // ServiceConnection "parameter must not be null."
            // https://developer.android.com/reference/android/content/Context.html#unbindService(android.content.ServiceConnection)
            mContext.unbindService(/* serviceConnection= */ this);
            mIsServiceBindingInitiated = false;
        }
        mHandler.removeCallbacksAndMessages(null);
    }
}
