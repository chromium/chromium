// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.mock_service;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.base.Log;
import org.chromium.components.ip_protection_auth.common.IErrorCode;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;

/** Mock IP Protection auth services with simple behaviors that ignore inputs */
public abstract class ConstantResponseService extends Service {
    private static final String TAG = "ConstResponseService";

    protected abstract void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
            throws RemoteException;

    protected abstract void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
            throws RemoteException;

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate for %s", this.getClass().getName());
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy for %s", this.getClass().getName());
    }

    @Override
    public IBinder onBind(Intent intent) {
        final String className = this.getClass().getName();
        Log.i(TAG, "returning %s binding for %s", className, intent.toString());
        return new IIpProtectionAuthService.Stub() {
            @Override
            public void getInitialData(byte[] bytes, IIpProtectionGetInitialDataCallback callback) {
                Log.i(TAG, "got getInitialData request for %s", className);
                try {
                    handleGetInitialData(callback);
                } catch (RemoteException e) {
                    Log.e(TAG, "failed to deliver %s getInitialData response", className, e);
                }
            }

            @Override
            public void authAndSign(byte[] bytes, IIpProtectionAuthAndSignCallback callback) {
                Log.i(TAG, "got authAndSign request for %s", className);
                try {
                    handleAuthAndSign(callback);
                } catch (RemoteException e) {
                    Log.e(TAG, "failed to deliver %s authAndSign response", className, e);
                }
            }
        };
    }

    public static class TransientError extends ConstantResponseService {
        @Override
        protected void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
                throws RemoteException {
            callback.reportError(IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
        }

        @Override
        protected void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
                throws RemoteException {
            callback.reportError(IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
        }
    }

    public static class PersistentError extends ConstantResponseService {
        @Override
        protected void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
                throws RemoteException {
            callback.reportError(IErrorCode.IP_PROTECTION_AUTH_SERVICE_PERSISTENT_ERROR);
        }

        @Override
        protected void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
                throws RemoteException {
            callback.reportError(IErrorCode.IP_PROTECTION_AUTH_SERVICE_PERSISTENT_ERROR);
        }
    }
}
