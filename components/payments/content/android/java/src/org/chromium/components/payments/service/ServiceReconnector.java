// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.service;

import android.os.Handler;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/**
 * A helper class for reconnecting to the remote service in the case of unexpected connection
 * failure. Uses exponential back off.
 */
@NullMarked
public class ServiceReconnector {
    private static final String TAG = "PayServiceReconnect";
    private static final int RETRY_DELAY_MILLISECONDS = 1000;
    private static final int EXPONENTIAL_BACKOFF_FACTOR = 2;
    private final Reconnectable mConnection;
    private final int mMaxRetryNumber;
    private final Handler mHandler;
    private int mRetryNumber;

    /**
     * Creates a service reconnector.
     *
     * @param connection The service connection to attempt to reconnect after an unexpected
     *     disconnect.
     * @param maxRetryNumber The maximum number of times that a reconnection should be attempted
     *     after unexpected disconnects.
     * @param handler The handler for posting tasks to run after a delay.
     */
    public ServiceReconnector(Reconnectable connection, int maxRetryNumber, Handler handler) {
        mConnection = connection;
        mMaxRetryNumber = maxRetryNumber;
        mHandler = handler;
    }

    /** Handle unexpected service disconnect. */
    public void onUnexpectedServiceDisconnect() {
        if (mRetryNumber >= mMaxRetryNumber) {
            Log.i(TAG, "Max reconnects reached.");
            mConnection.terminateConnection();
            return;
        }

        int delayMilliseconds =
                RETRY_DELAY_MILLISECONDS
                        * (int) Math.pow(EXPONENTIAL_BACKOFF_FACTOR, mRetryNumber++);
        Log.i(TAG, "%d: will reconnect in %d ms.", mRetryNumber, delayMilliseconds);

        mConnection.unbindService();
        mHandler.postDelayed(mConnection::connectToService, delayMilliseconds);
    }

    /** Handle intentional service disconnect. */
    public void onIntentionalServiceDisconnect() {
        mRetryNumber = mMaxRetryNumber;
        mHandler.removeCallbacksAndMessages(null);
    }
}
