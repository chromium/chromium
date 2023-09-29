// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.errorprone.annotations.DoNotCall;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.AuthAndSignRequest;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.AuthAndSignResponse;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.GetInitialDataRequest;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.GetInitialDataResponse;

/**
 * Client interface for the IP Protection Auth service.
 *
 * The methods in this class are thread-safe (except for close() which should not be called
 * concurrently with other methods).
 *
 * TODO(abhijithnair): Update documentation once production ready.
 * DO NOT DEPEND. CURRENTLY UNDER DEVELOPMENT.
 */
public final class IpProtectionAuthClient implements AutoCloseable {
    // Only used for testing.
    private static final String IP_PROTECTION_AUTH_STUB_SERVICE_NAME =
            "org.chromium.components.ip_protection_auth.mock_service.IpProtectionAuthServiceMock";

    // mService being null signifies that the object has been closed by calling close().
    @Nullable
    private IIpProtectionAuthService mService;
    // We need to store this to unbind from the service.
    @Nullable
    private ConnectionSetup mConnection;

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
            IpProtectionAuthClient ipProtectionClient = new IpProtectionAuthClient(
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
        }
    }

    private static final class IIpProtectionGetInitialDataCallbackStub
            extends IIpProtectionGetInitialDataCallback.Stub {
        private final GetInitialDataCallback mCallback;

        IIpProtectionGetInitialDataCallbackStub(GetInitialDataCallback callback) {
            mCallback = callback;
        }

        @Override
        public void reportResult(byte[] bytes) {
            try {
                mCallback.onResult(GetInitialDataResponse.parser().parseFrom(bytes));
            } catch (InvalidProtocolBufferException ex) {
                // TODO(abhijithnair): Handle this case correctly.
                throw new RuntimeException(ex);
            }
        }

        @Override
        public void reportError(byte[] bytes) {
            mCallback.onError(bytes);
        }
    }

    private static final class IIpProtectionAuthAndSignCallbackStub
            extends IIpProtectionAuthAndSignCallback.Stub {
        private final AuthAndSignCallback mCallback;

        IIpProtectionAuthAndSignCallbackStub(AuthAndSignCallback callback) {
            mCallback = callback;
        }

        @Override
        public void reportResult(byte[] bytes) {
            try {
                mCallback.onResult(AuthAndSignResponse.parser().parseFrom(bytes));
            } catch (InvalidProtocolBufferException ex) {
                // TODO(abhijithnair): Handle this case correctly.
                throw new RuntimeException(ex);
            }
        }

        @Override
        public void reportError(byte[] bytes) {
            mCallback.onError(bytes);
        }
    }

    public interface IpProtectionAuthServiceCallback {
        void onResult(IpProtectionAuthClient client);

        void onError(String error);
    }

    public interface GetInitialDataCallback {
        // TODO(abhijithnair): Consider using a non-proto generated class.
        void onResult(GetInitialDataResponse result);

        // TODO(abhijithnair): Change to using a error specific class.
        void onError(byte[] error);
    }

    public interface AuthAndSignCallback {
        // TODO(abhijithnair): Consider using a non-proto generated class.
        void onResult(AuthAndSignResponse result);

        // TODO(abhijithnair): Change to using a error specific class.
        void onError(byte[] error);
    }

    IpProtectionAuthClient(@NonNull ConnectionSetup connectionSetup,
            @NonNull IIpProtectionAuthService ipProtectionAuthService) {
        mConnection = connectionSetup;
        mService = ipProtectionAuthService;
    }

    @VisibleForTesting
    public static void createConnectedInstanceForTestingAsync(
            @NonNull Context context, @NonNull IpProtectionAuthServiceCallback callback) {
        ComponentName componentName =
                new ComponentName(context, IP_PROTECTION_AUTH_STUB_SERVICE_NAME);
        Intent intent = new Intent();
        intent.setComponent(componentName);
        ConnectionSetup connectionSetup = new ConnectionSetup(context, callback);
        context.bindService(intent, connectionSetup, Context.BIND_AUTO_CREATE);
    }

    @DoNotCall
    public static void createConnectedInstance(
            @NonNull Context context, @NonNull IpProtectionAuthServiceCallback callback) {
        // TODO(abhijithnair): Implement!
        throw new UnsupportedOperationException("unimplemented");
    }

    public void getInitialData(GetInitialDataRequest request, GetInitialDataCallback callback) {
        if (mService == null) {
            // This denotes a coding error by the caller so it makes sense to throw an unchecked
            // exception.
            throw new IllegalStateException("Already closed");
        }

        IIpProtectionGetInitialDataCallbackStub callbackStub =
                new IIpProtectionGetInitialDataCallbackStub(callback);
        try {
            mService.getInitialData(request.toByteArray(), callbackStub);
        } catch (RemoteException ex) {
            // TODO(abhijithnair): Handle this case correctly.
        }
    }

    public void authAndSign(AuthAndSignRequest request, AuthAndSignCallback callback) {
        if (mService == null) {
            // This denotes a coding error by the caller so it makes sense to throw an unchecked
            // exception.
            throw new IllegalStateException("Already closed");
        }
        IIpProtectionAuthAndSignCallbackStub callbackStub =
                new IIpProtectionAuthAndSignCallbackStub(callback);
        try {
            mService.authAndSign(request.toByteArray(), callbackStub);
        } catch (RemoteException ex) {
            // TODO(abhijithnair): Handle this case correctly.
        }
    }

    @Override
    public void close() {
        mConnection.mContext.unbindService(mConnection);
        mConnection = null;
        mService = null;
    }
}
