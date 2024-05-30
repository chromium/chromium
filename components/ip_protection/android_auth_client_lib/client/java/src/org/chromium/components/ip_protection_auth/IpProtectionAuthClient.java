// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;

/**
 * Client interface for the IP Protection Auth service.
 *
 * <p>The methods in this class are thread-safe (except for close() which should not be called
 * concurrently with other methods).
 */
@JNINamespace("ip_protection::android")
public final class IpProtectionAuthClient implements AutoCloseable {
    // Only used for testing.
    private static final String IP_PROTECTION_AUTH_MOCK_CLASS_NAME =
            "org.chromium.components.ip_protection_auth.mock_service.IpProtectionAuthServiceMock";
    private static final String IP_PROTECTION_AUTH_PACKAGE_NAME =
            "org.chromium.components.ip_protection_auth";
    private static final String IP_PROTECTION_AUTH_ACTION_NAME =
            "android.net.http.IpProtectionAuthService";

    // mService being null signifies that the object has been closed by calling close().
    @Nullable private IIpProtectionAuthService mService;
    // We need to store this to unbind from the service.
    @Nullable private ConnectionSetup mConnection;

    private static final class ConnectionSetup implements ServiceConnection {
        private final IpProtectionAuthServiceCallback mCallback;
        private final Context mContext;

        ConnectionSetup(
                @NonNull Context context, @NonNull IpProtectionAuthServiceCallback callback) {
            mContext = context;
            mCallback = callback;
        }

        @Override
        public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
            IpProtectionAuthClient ipProtectionClient =
                    new IpProtectionAuthClient(
                            this, IIpProtectionAuthService.Stub.asInterface(iBinder));
            mCallback.onResult(ipProtectionClient);
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            mContext.unbindService(this);
        }

        @Override
        public void onBindingDied(ComponentName name) {
            mContext.unbindService(this);
        }

        @Override
        public void onNullBinding(ComponentName name) {
            mContext.unbindService(this);
            mCallback.onError("Service returned null from onBind()");
        }
    }

    private static final class IIpProtectionGetInitialDataCallbackStub
            extends IIpProtectionGetInitialDataCallback.Stub {
        private final IpProtectionByteArrayCallback mCallback;

        IIpProtectionGetInitialDataCallbackStub(IpProtectionByteArrayCallback callback) {
            mCallback = callback;
        }

        @Override
        public void reportResult(byte[] bytes) {
            mCallback.onResult(bytes);
        }

        @Override
        public void reportError(byte[] bytes) {
            mCallback.onError(bytes);
        }
    }

    private static final class IIpProtectionAuthAndSignCallbackStub
            extends IIpProtectionAuthAndSignCallback.Stub {
        private final IpProtectionByteArrayCallback mCallback;

        IIpProtectionAuthAndSignCallbackStub(IpProtectionByteArrayCallback callback) {
            mCallback = callback;
        }

        @Override
        public void reportResult(byte[] bytes) {
            mCallback.onResult(bytes);
        }

        @Override
        public void reportError(byte[] bytes) {
            mCallback.onError(bytes);
        }
    }

    IpProtectionAuthClient(
            @NonNull ConnectionSetup connectionSetup,
            @NonNull IIpProtectionAuthService ipProtectionAuthService) {
        mConnection = connectionSetup;
        mService = ipProtectionAuthService;
    }

    @VisibleForTesting
    @CalledByNative
    public static void createConnectedInstanceForTestingAsync(
            @NonNull IpProtectionAuthServiceCallback callback) {
        var context = ContextUtils.getApplicationContext();
        Intent intent = new Intent();
        intent.setClassName(IP_PROTECTION_AUTH_PACKAGE_NAME, IP_PROTECTION_AUTH_MOCK_CLASS_NAME);
        ConnectionSetup connectionSetup = new ConnectionSetup(context, callback);
        context.bindService(intent, connectionSetup, Context.BIND_AUTO_CREATE);
    }

    @CalledByNative
    public static void createConnectedInstance(@NonNull IpProtectionAuthServiceCallback callback)
            throws RemoteException {
        // Use IP_PROTECTION_AUTH_ACTION_NAME to resolve system service that satisfies
        // the intent, going from implicit to explicit intent.
        var context = ContextUtils.getApplicationContext();
        var packageManager = context.getPackageManager();
        var intent = new Intent(IP_PROTECTION_AUTH_ACTION_NAME);
        // When Chromium moves to API level 33 as minimum-supported version,
        // we can switch resolveService to use PackageManager.ResolveInfoFlags.
        var resolveInfo = packageManager.resolveService(intent, PackageManager.MATCH_SYSTEM_ONLY);
        if (resolveInfo == null || resolveInfo.serviceInfo == null) {
            // TODO(b/328780742): use callback.onError instead of throwing
            throw new RemoteException(
                    "Unable to locate the IP Protection authentication provider package ("
                            + IP_PROTECTION_AUTH_ACTION_NAME
                            + " action). This is expected if the host system is not set up to"
                            + " provide IP Protection services.");
        }
        var serviceInfo = resolveInfo.serviceInfo;
        var componentName = new ComponentName(serviceInfo.packageName, serviceInfo.name);
        intent.setComponent(componentName);

        var connectionSetup = new ConnectionSetup(context, callback);
        try {
            boolean binding =
                    context.bindService(intent, connectionSetup, Context.BIND_AUTO_CREATE);
            if (binding) {
                return;
            } else {
                context.unbindService(connectionSetup);
                throw new RemoteException("bindService() failed: returned false");
            }
        } catch (SecurityException e) {
            context.unbindService(connectionSetup);
            throw new RemoteException("Failed to bind service: " + e);
        }
    }

    @CalledByNative
    public void getInitialData(byte[] request, IpProtectionByteArrayCallback callback) {
        if (mService == null) {
            // This denotes a coding error by the caller so it makes sense to throw an
            // unchecked exception.
            throw new IllegalStateException("Already closed");
        }

        IIpProtectionGetInitialDataCallbackStub callbackStub =
                new IIpProtectionGetInitialDataCallbackStub(callback);
        try {
            mService.getInitialData(request, callbackStub);
        } catch (RemoteException ex) {
            // TODO(abhijithnair): Handle this case correctly.
        }
    }

    @CalledByNative
    public void authAndSign(byte[] request, IpProtectionByteArrayCallback callback) {
        if (mService == null) {
            // This denotes a coding error by the caller so it makes sense to throw an
            // unchecked exception.
            throw new IllegalStateException("Already closed");
        }
        IIpProtectionAuthAndSignCallbackStub callbackStub =
                new IIpProtectionAuthAndSignCallbackStub(callback);
        try {
            mService.authAndSign(request, callbackStub);
        } catch (RemoteException ex) {
            // TODO(abhijithnair): Handle this case correctly.
        }
    }

    @Override
    @CalledByNative
    public void close() {
        mConnection.mContext.unbindService(mConnection);
        mConnection = null;
        mService = null;
    }
}
