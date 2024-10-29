// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import androidx.annotation.NonNull;

/**
 * A helper to connect to the payment app's service that dynamically updates payment details (e.g.,
 * total price) based on user's payment method, shipping address, or shipping options. The
 * connection stays open until terminated.
 */
public class PaymentDetailsUpdateConnection implements ServiceConnection {
    @NonNull private final Context mContext;
    @NonNull private final Intent mPaymentAppServiceIntent;
    @NonNull private final IPaymentDetailsUpdateService.Stub mChromiumService;
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
     */
    public PaymentDetailsUpdateConnection(
            @NonNull Context context,
            @NonNull Intent paymentAppServiceIntent,
            @NonNull IPaymentDetailsUpdateService.Stub chromiumService) {
        assert context != null;
        assert paymentAppServiceIntent != null;
        assert chromiumService != null;
        mContext = context;
        mPaymentAppServiceIntent = paymentAppServiceIntent;
        mChromiumService = chromiumService;
    }

    /** Connect to the service. */
    public void connectToService() {
        mIsBindingInitiated = true;
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
            terminateConnection();
        }
    }

    // ServiceConnection:
    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        if (service == null) {
            terminateConnection();
            return;
        }

        IPaymentDetailsUpdateServiceCallback paymentAppService =
                IPaymentDetailsUpdateServiceCallback.Stub.asInterface(service);
        if (paymentAppService == null) {
            terminateConnection();
            return;
        }

        try {
            paymentAppService.setPaymentDetailsUpdateService(mChromiumService);
        } catch (Throwable e) {
            // Many undocumented exceptions are not caught in the remote Service but passed on
            // to the Service caller, see writeException in Parcel.java.
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
        terminateConnection();
    }

    // ServiceConnection implementation:
    @Override
    public void onNullBinding(ComponentName name) {
        // "The app that requested the binding must still call
        // Context#unbindService(ServiceConnection) to release the tracking resources associated
        // with this ServiceConnection even if this callback was invoked following
        // Context.bindService() bindService()."
        // https://developer.android.com/reference/android/content/ServiceConnection#onNullBinding(android.content.ComponentName)
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
        terminateConnection();
    }

    /** Disconnect from the service, if still connected, and release the tracking resources. */
    public void terminateConnection() {
        if (mIsBindingInitiated) {
            mContext.unbindService(/* serviceConnection= */ this);
            mIsBindingInitiated = false;
        }
    }
}
