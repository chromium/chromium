// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.payments.service.Reconnectable;
import org.chromium.components.payments.service.ServiceReconnector;

/**
 * A helper to connect to the payment app's service that dynamically updates payment details (e.g.,
 * total price) based on user's payment method, shipping address, or shipping options. The
 * connection stays open until terminated.
 */
@NullMarked
public class PaymentDetailsUpdateConnection implements ServiceConnection, Reconnectable {
    private static final String TAG = "PaymentDetailUpdate";
    private final Context mContext;
    private final Intent mPaymentAppServiceIntent;
    private final IPaymentDetailsUpdateService.Stub mChromiumService;
    private final String mServiceName;
    private final ServiceReconnector mServiceReconnector;
    private boolean mIsBindingInitiated;

    /**
     * Initialize the connection helper.
     *
     * @param context The application context. Should not be null.
     * @param paymentAppServiceIntent The intent into the service of the payment app, as created by
     *     {@link WebPaymentIntentHelper#createUpdatePaymentDetailsServiceIntent}. Should not be
     *     null.
     * @param chromiumService The service in Chromium that receives the updates to user's payment
     *     method, shipping address, and shipping option. Should not be null.
     * @param maxRetryNumber The maximum number of times to attempt to reconnect in case of an
     *     unexpected disconnect.
     */
    public PaymentDetailsUpdateConnection(
            Context context,
            Intent paymentAppServiceIntent,
            IPaymentDetailsUpdateService.Stub chromiumService,
            int maxRetryNumber) {
        assert context != null;
        assert paymentAppServiceIntent != null;
        assert chromiumService != null;
        mContext = context;
        mPaymentAppServiceIntent = paymentAppServiceIntent;
        mChromiumService = chromiumService;
        mServiceName =
                mPaymentAppServiceIntent != null && mPaymentAppServiceIntent.getComponent() != null
                        ? mPaymentAppServiceIntent.getComponent().getClassName()
                        : "";
        mServiceReconnector = new ServiceReconnector(this, maxRetryNumber, new Handler());
    }

    // Reconnectable:
    @Override
    public void connectToService() {
        mIsBindingInitiated = true;
        Log.i(TAG, "Connecting to \"%s\".", mServiceName);
        try {
            // "Regardless of the return value, you should later call
            // unbindService(ServiceConnection) to release the connection."
            // https://developer.android.com/reference/android/content/Context#bindService(android.content.Intent,%20android.content.ServiceConnection,%20int)
            mContext.bindService(
                    mPaymentAppServiceIntent,
                    /* serviceConnection= */ this,
                    Context.BIND_AUTO_CREATE);
        } catch (SecurityException e) {
            // "If the caller does not have permission to access the service or the service cannot
            // be found. Call unbindService(ServiceConnection) to release the connection when this
            // exception is thrown."
            // https://developer.android.com/reference/android/content/Context#bindService(android.content.Intent,%20android.content.ServiceConnection,%20int)
            Log.e(
                    TAG,
                    "No permission to connect to \"%s\" or it cannot be found: %s",
                    mServiceName,
                    e.getMessage());
            terminateConnection();
        }
    }

    // ServiceConnection:
    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        if (service == null) {
            Log.e(TAG, "Null service \"%s\".", mServiceName);
            terminateConnection();
            return;
        }

        IPaymentDetailsUpdateServiceCallback paymentAppService =
                IPaymentDetailsUpdateServiceCallback.Stub.asInterface(service);
        if (paymentAppService == null) {
            Log.e(TAG, "Mismatched service interface \"%s\".", mServiceName);
            terminateConnection();
            return;
        }

        Log.i(TAG, "Sending payment details update service to \"%s\".", mServiceName);
        try {
            paymentAppService.setPaymentDetailsUpdateService(mChromiumService);
        } catch (Throwable e) {
            // Many undocumented exceptions are not caught in the remote Service but passed on
            // to the Service caller, see writeException in Parcel.java.
            Log.e(TAG, "Exception in remote service \"%s\": %s", mServiceName, e.getMessage());
            terminateConnection();
        }
    }

    // ServiceConnection implementation:
    @Override
    public void onServiceDisconnected(ComponentName name) {
        // "Called when a connection to the Service has been lost. This typically happens when the
        // process hosting the service has crashed or been killed. This does not remove the
        // ServiceConnection itself -- this binding to the service will remain active, and you will
        // receive a call to onServiceConnected(ComponentName, IBinder) when the Service is next
        // running."
        // https://developer.android.com/reference/android/content/ServiceConnection#onServiceDisconnected(android.content.ComponentName)
        Log.i(TAG, "\"%s\" disconnected.", mServiceName);
        mServiceReconnector.onUnexpectedServiceDisconnect();
    }

    // ServiceConnection implementation:
    @Override
    public void onNullBinding(ComponentName name) {
        // "The app that requested the binding must still call
        // Context#unbindService(ServiceConnection) to release the tracking resources associated
        // with this ServiceConnection even if this callback was invoked following
        // Context.bindService() bindService()."
        // https://developer.android.com/reference/android/content/ServiceConnection#onNullBinding(android.content.ComponentName)
        Log.e(TAG, "Null binding for service \"%s\".", mServiceName);
        terminateConnection();
    }

    // ServiceConnection implementation:
    @Override
    public void onBindingDied(ComponentName name) {
        // "The app that requested the binding must still call
        // Context#unbindService(ServiceConnection) to release the tracking resources associated
        // with this ServiceConnection even if this callback was invoked following
        // Context.bindService() bindService()."
        // https://developer.android.com/reference/android/content/ServiceConnection#onBindingDied(android.content.ComponentName)
        Log.e(TAG, "\"%s\" binding died.", mServiceName);
        mServiceReconnector.onUnexpectedServiceDisconnect();
    }

    // Reconnectable:
    @Override
    public void terminateConnection() {
        mServiceReconnector.onIntentionalServiceDisconnect();
        if (mIsBindingInitiated) {
            Log.i(TAG, "Terminating connection to \"%s\".", mServiceName);
            unbindService();
            mIsBindingInitiated = false;
        }
    }

    // Reconnectable:
    @Override
    public void unbindService() {
        mContext.unbindService(/* serviceConnection= */ this);
    }
}
