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
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
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
    // For testing only
    private static final String IP_PROTECTION_AUTH_MOCK_CLASS_NAME =
            "org.chromium.components.ip_protection_auth.mock_service.IpProtectionAuthServiceMock";
    // For testing only
    private static final String IP_PROTECTION_AUTH_MOCK_PACKAGE_NAME =
            "org.chromium.components.ip_protection_auth";

    private static final String IP_PROTECTION_AUTH_ACTION =
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
    @CalledByNativeForTesting
    public static void createConnectedInstanceForTestingAsync(
            @NonNull IpProtectionAuthServiceCallback callback) {
        Intent intent = new Intent();
        intent.setClassName(
                IP_PROTECTION_AUTH_MOCK_PACKAGE_NAME, IP_PROTECTION_AUTH_MOCK_CLASS_NAME);
        createConnectedInstanceForTestingAsync(intent, callback);
    }

    @VisibleForTesting
    public static void createConnectedInstanceForTestingAsync(
            @NonNull Intent intent, @NonNull IpProtectionAuthServiceCallback callback) {
        createConnectedInstanceCommon(intent, PackageManager.MATCH_DISABLED_COMPONENTS, callback);
    }

    @CalledByNative
    public static void createConnectedInstance(@NonNull IpProtectionAuthServiceCallback callback) {
        // Use IP_PROTECTION_AUTH_ACTION to resolve system service that satisfies
        // the intent, going from implicit to explicit intent in createConnectedInstanceCommon.
        var intent = new Intent(IP_PROTECTION_AUTH_ACTION);
        createConnectedInstanceCommon(intent, PackageManager.MATCH_SYSTEM_ONLY, callback);
    }

    /**
     * Converts the given intent to an explicit intent and binds to the service.
     *
     * <p>The intent is converted to an explicit intent by resolving via the PackageManager. The
     * callback is called with either an IpProtectionAuthClient bound to the service or an error
     * string.
     */
    private static void createConnectedInstanceCommon(
            @NonNull Intent intent,
            int resolveFlags,
            @NonNull IpProtectionAuthServiceCallback callback) {
        var context = ContextUtils.getApplicationContext();
        var packageManager = context.getPackageManager();
        // When Chromium moves to API level 33 as minimum-supported version,
        // we can switch resolveService to use PackageManager.ResolveInfoFlags.
        var resolveInfo = packageManager.resolveService(intent, resolveFlags);
        if (resolveInfo == null || resolveInfo.serviceInfo == null) {
            final String error =
                    "Unable to locate the IP Protection authentication provider package ("
                            + intent.getAction()
                            + " action). This is expected if the host system is not set up to"
                            + " provide IP Protection services.";
            ThreadUtils.postOnUiThread(() -> callback.onError(error));
            return;
        }
        var serviceInfo = resolveInfo.serviceInfo;
        var componentName = new ComponentName(serviceInfo.packageName, serviceInfo.name);
        intent.setComponent(componentName);
        var connectionSetup = new ConnectionSetup(context, callback);
        try {
            boolean binding =
                    context.bindService(intent, connectionSetup, Context.BIND_AUTO_CREATE);
            if (!binding) {
                context.unbindService(connectionSetup);
                ThreadUtils.postOnUiThread(
                        () -> callback.onError("bindService() failed: returned false"));
                return;
            }
        } catch (SecurityException e) {
            context.unbindService(connectionSetup);
            ThreadUtils.postOnUiThread(() -> callback.onError("Failed to bind service: " + e));
            return;
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
