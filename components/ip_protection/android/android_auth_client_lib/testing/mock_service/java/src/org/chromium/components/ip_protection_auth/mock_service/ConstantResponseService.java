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
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetProxyConfigCallback;

/** Mock IP Protection auth services with simple behaviors that ignore inputs */
public abstract class ConstantResponseService extends Service {
    private static final String TAG = "ConstResponseService";

    protected abstract void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
            throws RemoteException;

    protected abstract void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
            throws RemoteException;

    protected abstract void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
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

            @Override
            public void getProxyConfig(byte[] bytes, IIpProtectionGetProxyConfigCallback callback) {
                Log.i(TAG, "got getProxyConfig request for %s", className);
                try {
                    handleGetProxyConfig(callback);
                } catch (RemoteException e) {
                    Log.e(TAG, "failed to deliver %s getProxyConfig response", className, e);
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

        @Override
        protected void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
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

        @Override
        protected void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
                throws RemoteException {
            callback.reportError(IErrorCode.IP_PROTECTION_AUTH_SERVICE_PERSISTENT_ERROR);
        }
    }

    public static class IllegalErrorCode extends ConstantResponseService {
        @Override
        protected void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
                throws RemoteException {
            callback.reportError(-1);
        }

        @Override
        protected void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
                throws RemoteException {
            callback.reportError(-1);
        }

        @Override
        protected void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
                throws RemoteException {
            callback.reportError(-1);
        }
    }

    public static class Null extends ConstantResponseService {
        @Override
        protected void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
                throws RemoteException {
            callback.reportResult(null);
        }

        @Override
        protected void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
                throws RemoteException {
            callback.reportResult(null);
        }

        @Override
        protected void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
                throws RemoteException {
            callback.reportResult(null);
        }
    }

    public static class Unparsable extends ConstantResponseService {
        @Override
        protected void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
                throws RemoteException {
            callback.reportResult("unparsable non-proto gibberish".getBytes());
        }

        @Override
        protected void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
                throws RemoteException {
            callback.reportResult("unparsable non-proto gibberish".getBytes());
        }

        @Override
        protected void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
                throws RemoteException {
            callback.reportResult("unparsable non-proto gibberish".getBytes());
        }
    }

    public static class SynchronousError extends ConstantResponseService {
        @Override
        protected void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
                throws RemoteException {
            throw new SecurityException("Intentional security exception for testing");
        }

        @Override
        protected void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
                throws RemoteException {
            throw new SecurityException("Intentional security exception for testing");
        }

        @Override
        protected void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
                throws RemoteException {
            throw new SecurityException("Intentional security exception for testing");
        }
    }

    public static class NeverResolve extends ConstantResponseService {
        @Override
        protected void handleGetInitialData(IIpProtectionGetInitialDataCallback callback)
                throws RemoteException {}

        @Override
        protected void handleAuthAndSign(IIpProtectionAuthAndSignCallback callback)
                throws RemoteException {}

        @Override
        protected void handleGetProxyConfig(IIpProtectionGetProxyConfigCallback callback)
                throws RemoteException {}
    }
}
