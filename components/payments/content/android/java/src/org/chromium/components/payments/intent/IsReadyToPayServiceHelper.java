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
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A helper to query the payment app's IsReadyToPay service. */
@NullMarked
public class IsReadyToPayServiceHelper extends IsReadyToPayServiceCallback.Stub
        implements ServiceConnection {
    private static final String TAG = "IsReadyToPayService";

    /** The maximum number of milliseconds to wait for a response from a READY_TO_PAY service. */
    private long mReadyToPayTimeoutMs = 2000;

    /** The maximum number of milliseconds to wait for a connection to READY_TO_PAY service. */
    private long mServiceConnectionTimeoutMs = 5000;

    private final Context mContext;

    // This callback can be used only once, set to null after that.
    private @Nullable ResultHandler mResultHandler;

    private boolean mIsServiceBindingInitiated;
    private boolean mIsServiceConnected;
    private final Handler mHandler;
    private final Intent mIsReadyToPayIntent;
    private final String mServiceName;

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
        mServiceName =
                mIsReadyToPayIntent != null && mIsReadyToPayIntent.getComponent() != null
                        ? mIsReadyToPayIntent.getComponent().getClassName()
                        : "";
    }

    /**
     * Query the IsReadyToPay service. The result would be returned in the resultHandler callback
     * asynchronously. Note that resultHandler would be invoked only once.
     */
    public void query() {
        Log.i(TAG, "Connecting to \"%s\".", mServiceName);
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
            if (!mIsServiceBindingInitiated) {
                Log.e(TAG, "Could not find \"%s\" or no permission to connect.", mServiceName);
            }
        } catch (SecurityException e) {
            Log.e(TAG, "Error connecting to \"%s\": %s", mServiceName, e.getMessage());
            // Intentionally blank, so mIsServiceBindingInitiated is false.
        }

        if (!mIsServiceBindingInitiated) {
            reportError();
            return;
        }

        mHandler.postDelayed(
                () -> {
                    if (!mIsServiceConnected) {
                        Log.e(TAG, "Timeout connecting to \"%s\".", mServiceName);
                        reportError();
                    }
                },
                mServiceConnectionTimeoutMs);
    }

    // ServiceConnection:
    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        // Timeout could cause the null.
        if (mResultHandler == null) return;

        mIsServiceConnected = true;

        IsReadyToPayService isReadyToPayService = IsReadyToPayService.Stub.asInterface(service);
        if (isReadyToPayService == null) {
            Log.e(TAG, "Interface mismatch in \"%s\".", mServiceName);
            reportError();
            return;
        }

        Log.i(TAG, "Querying \"%s\".", mServiceName);
        try {
            isReadyToPayService.isReadyToPay(/* callback= */ this);
        } catch (Throwable e) {
            // Many undocumented exceptions are not caught in the remote Service but passed on
            // to the Service caller, see writeException in Parcel.java.
            Log.e(TAG, "Error in remote service \"%s\": %s.", mServiceName, e.getMessage());
            reportError();
            return;
        }
        mHandler.postDelayed(this::reportError, mReadyToPayTimeoutMs);
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
        Log.i(TAG, "\"%s\" disconnected.", mServiceName);
        reportError();
    }

    // IsReadyToPayServiceCallback.Stub:
    @Override
    public void handleIsReadyToPay(boolean isReadyToPay) throws RemoteException {
        if (mResultHandler == null) return;
        if (isReadyToPay) {
            Log.i(TAG, "\"%s\": Ready to pay.", mServiceName);
        } else {
            Log.e(TAG, "\"%s\": Not ready to pay.", mServiceName);
        }
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
            Log.i(TAG, "Terminating connection to \"%s\".", mServiceName);
            mContext.unbindService(/* serviceConnection= */ this);
            mIsServiceBindingInitiated = false;
        }
        mHandler.removeCallbacksAndMessages(null);
    }

    /**
     * @param timeoutForTest The number of milliseconds to use for timeouts in tests.
     */
    public void setTimeoutsMsForTesting(long timeoutForTesting) {
        mServiceConnectionTimeoutMs = timeoutForTesting;
        mReadyToPayTimeoutMs = timeoutForTesting;
    }
}
