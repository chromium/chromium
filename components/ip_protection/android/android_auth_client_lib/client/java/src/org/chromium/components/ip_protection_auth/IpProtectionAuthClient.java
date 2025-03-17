// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.ip_protection_auth.common.IErrorCode;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetProxyConfigCallback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.Set;

/** Client interface for the IP Protection Auth service. */
@JNINamespace("ip_protection::android")
@NullMarked
public final class IpProtectionAuthClient implements AutoCloseable {
    private static final String TAG = "IppAuthClient";

    private static final String IP_PROTECTION_AUTH_ACTION =
            "android.net.http.IpProtectionAuthService";

    // mService being null signifies that the object has been closed by calling close().
    private @Nullable IIpProtectionAuthService mService;
    // We need to store this to unbind from the service.
    private @Nullable ConnectionSetup mConnection;
    final CallbackTracker mCallbackTracker;

    // These values must be kept in sync with AuthRequestError in
    // ip_protection_auth_client_interface.h
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({AuthRequestError.TRANSIENT, AuthRequestError.PERSISTENT, AuthRequestError.OTHER})
    public @interface AuthRequestError {
        int TRANSIENT = 0;
        int PERSISTENT = 1;
        int OTHER = 2;
    }

    /**
     * Convert IErrorCode value to AuthRequestError value.
     *
     * <p>Invalid values are mapped to AuthRequestError.OTHER.
     */
    private static int convertErrorCodeToAuthRequestError(int errorCode) {
        switch (errorCode) {
            case IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR:
                return AuthRequestError.TRANSIENT;
            case IErrorCode.IP_PROTECTION_AUTH_SERVICE_PERSISTENT_ERROR:
                return AuthRequestError.PERSISTENT;
            default:
                return AuthRequestError.OTHER;
        }
    }

    /** This class must be used exclusively from the main thread. */
    private static final class ConnectionSetup implements ServiceConnection {
        private final Context mContext;
        private @Nullable IpProtectionAuthServiceCallback mCallback;
        private @Nullable IpProtectionAuthClient mIpProtectionClient;
        private boolean mBound;

        ConnectionSetup(Context context, IpProtectionAuthServiceCallback callback) {
            mContext = context;
            mCallback = callback;
            mIpProtectionClient = null;
            mBound = true;
        }

        @Override
        public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
            try (TraceEvent event =
                    TraceEvent.scoped("IpProtectionAuthClient.Create.OnServiceConnected")) {
                // In some odd cases (b/357770633), this lifecycle method is triggered even
                // after the service is unbound. We ensure that the callback cannot be invoked more
                // than once from this object if it so happens.
                if (mCallback == null) {
                    return;
                }
                mIpProtectionClient =
                        new IpProtectionAuthClient(
                                this, IIpProtectionAuthService.Stub.asInterface(iBinder));
                mCallback.onResult(mIpProtectionClient);
                mCallback = null;
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            try (TraceEvent event =
                    TraceEvent.scoped("IpProtectionAuthClient.Create.OnServiceDisconnected")) {
                unbindIfBound();
                if (mIpProtectionClient != null) {
                    mIpProtectionClient.mCallbackTracker.rejectUnresolvedCallbacks(
                            AuthRequestError.OTHER);
                }
            }
        }

        @Override
        public void onBindingDied(ComponentName name) {
            try (TraceEvent event =
                    TraceEvent.scoped("IpProtectionAuthClient.Create.OnBindingDied")) {
                unbindIfBound();
                if (mIpProtectionClient != null) {
                    mIpProtectionClient.mCallbackTracker.rejectUnresolvedCallbacks(
                            AuthRequestError.OTHER);
                }
            }
        }

        @Override
        public void onNullBinding(ComponentName name) {
            try (TraceEvent event =
                    TraceEvent.scoped("IpProtectionAuthClient.Create.OnNullBinding")) {
                unbindIfBound();
                // We ensure that the callback cannot be invoked more than once from this object.
                if (mCallback == null) {
                    return;
                }
                mCallback.onError("Service returned null from onBind()");
                mCallback = null;
            }
        }

        public void unbindIfBound() {
            ThreadUtils.assertOnUiThread();
            if (mBound) {
                mBound = false;
                mContext.unbindService(this);
            }
        }
    }

    /**
     * Enforces single-run semantics for callbacks across potentially competing threads and provides
     * mechanisms for running callbacks for abandoned requests.
     */
    static final class CallbackTracker {
        // Callbacks must never be run from code holding this lock.
        private final Object mUnresolvedCallbacksLock;

        @GuardedBy("mUnresolvedCallbacksLock")
        final Set<IpProtectionByteArrayCallback> mUnresolvedCallbacks;

        /**
         * A thread-safe wrapper around a callback that enforces that it's used (at most) once.
         *
         * <p>Note that this only enforces thread-safety for deciding whether a callback should be
         * run (in the case where there are threads competing to call the same callback). It does
         * not enforce any thread-safety for the underlying callback's logic.
         */
        public final class TrackedCallback implements IpProtectionByteArrayCallback {
            private final IpProtectionByteArrayCallback mInner;

            // See CallbackTracker.wrapCallback.
            public TrackedCallback(IpProtectionByteArrayCallback inner) {
                mInner = inner;
                synchronized (mUnresolvedCallbacksLock) {
                    mUnresolvedCallbacks.add(inner);
                }
            }

            @Override
            public void onResult(byte[] result) {
                IpProtectionByteArrayCallback callback = unwrap();
                if (callback != null) {
                    callback.onResult(result);
                }
            }

            @Override
            public void onError(int authRequestError) {
                IpProtectionByteArrayCallback callback = unwrap();
                if (callback != null) {
                    callback.onError(authRequestError);
                }
            }

            /**
             * Extract the wrapped callback.
             *
             * <p>The previously wrapped callback will no longer be tracked.
             *
             * @return The unwrapped callback if the callback has not bean consumed yet (unwrapped
             *     or triggered), or null if the callback has already been consumed. Null does not
             *     necessarily indicate a programming error - merely that the callback was
             *     resolved/rejected due to some other mechanism first, perhaps on a competing
             *     thread.
             */
            public @Nullable IpProtectionByteArrayCallback unwrap() {
                final boolean isUnresolved;
                synchronized (mUnresolvedCallbacksLock) {
                    isUnresolved = mUnresolvedCallbacks.remove(mInner);
                }
                if (!isUnresolved) {
                    Log.w(TAG, "callback already used");
                    return null;
                }
                return mInner;
            }
        }

        public CallbackTracker() {
            mUnresolvedCallbacksLock = new Object();
            mUnresolvedCallbacks = new HashSet<>();
        }

        /**
         * Wraps a callback in a thread-safe wrapper that ensures the callback is used only once.
         *
         * <p>The wrapped callback should be considered owned by the callback tracker and thus only
         * be used via the wrapper or the callback tracker.
         */
        public TrackedCallback wrapCallback(IpProtectionByteArrayCallback callback) {
            return new TrackedCallback(callback);
        }

        /**
         * Unwraps the callbacks from all unresolved tracked callbacks and returns the unwrapped
         * callbacks.
         */
        private IpProtectionByteArrayCallback[] unwrapAllCallbacks() {
            final IpProtectionByteArrayCallback[] callbacks;
            synchronized (mUnresolvedCallbacksLock) {
                callbacks = mUnresolvedCallbacks.toArray(new IpProtectionByteArrayCallback[0]);
                mUnresolvedCallbacks.clear();
            }
            return callbacks;
        }

        /** Call onError for all unresolved callbacks. */
        public void rejectUnresolvedCallbacks(int authRequestError) {
            for (IpProtectionByteArrayCallback callback : unwrapAllCallbacks()) {
                callback.onError(authRequestError);
            }
        }
    }

    private static final class IIpProtectionGetInitialDataCallbackStub
            extends IIpProtectionGetInitialDataCallback.Stub {
        private final CallbackTracker.TrackedCallback mCallback;

        IIpProtectionGetInitialDataCallbackStub(CallbackTracker.TrackedCallback callback) {
            mCallback = callback;
        }

        @Override
        public void reportResult(byte[] bytes) {
            if (bytes == null) {
                Log.e(TAG, "null getInitialData response");
                mCallback.onError(AuthRequestError.OTHER);
                return;
            }
            mCallback.onResult(bytes);
        }

        @Override
        public void reportError(int errorCode) {
            mCallback.onError(convertErrorCodeToAuthRequestError(errorCode));
        }
    }

    private static final class IIpProtectionAuthAndSignCallbackStub
            extends IIpProtectionAuthAndSignCallback.Stub {
        private final CallbackTracker.TrackedCallback mCallback;

        IIpProtectionAuthAndSignCallbackStub(CallbackTracker.TrackedCallback callback) {
            mCallback = callback;
        }

        @Override
        public void reportResult(byte[] bytes) {
            if (bytes == null) {
                Log.e(TAG, "null authAndSign response");
                mCallback.onError(AuthRequestError.OTHER);
                return;
            }
            mCallback.onResult(bytes);
        }

        @Override
        public void reportError(int errorCode) {
            mCallback.onError(convertErrorCodeToAuthRequestError(errorCode));
        }
    }

    private static final class IIpProtectionGetProxyConfigCallbackStub
            extends IIpProtectionGetProxyConfigCallback.Stub {
        private final CallbackTracker.TrackedCallback mCallback;

        IIpProtectionGetProxyConfigCallbackStub(CallbackTracker.TrackedCallback callback) {
            mCallback = callback;
        }

        @Override
        public void reportResult(byte[] bytes) {
            if (bytes == null) {
                Log.e(TAG, "null getProxyConfig response");
                mCallback.onError(AuthRequestError.OTHER);
                return;
            }
            mCallback.onResult(bytes);
        }

        @Override
        public void reportError(int errorCode) {
            mCallback.onError(convertErrorCodeToAuthRequestError(errorCode));
        }
    }

    IpProtectionAuthClient(
            ConnectionSetup connectionSetup, IIpProtectionAuthService ipProtectionAuthService) {
        mConnection = connectionSetup;
        mService = ipProtectionAuthService;
        mCallbackTracker = new CallbackTracker();
    }

    @CalledByNativeForTesting
    public static void createConnectedInstanceForTesting(
            String mockServicePackageName,
            String mockServiceClassName,
            IpProtectionAuthServiceCallback callback) {
        Intent intent = new Intent(IP_PROTECTION_AUTH_ACTION);
        intent.setClassName(mockServicePackageName, mockServiceClassName);
        createConnectedInstanceCommon(intent, PackageManager.MATCH_DISABLED_COMPONENTS, callback);
    }

    @CalledByNative
    public static void createConnectedInstance(IpProtectionAuthServiceCallback callback) {
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
            Intent intent, int resolveFlags, IpProtectionAuthServiceCallback callback) {
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
            ThreadUtils.postOnUiThread(
                    () -> {
                        try (TraceEvent event =
                                TraceEvent.scoped(
                                        "IpProtectionAuthClient.Create.FailResolveInfo")) {
                            callback.onError(error);
                        }
                    });
            return;
        }
        var serviceInfo = resolveInfo.serviceInfo;
        var componentName = new ComponentName(serviceInfo.packageName, serviceInfo.name);
        intent.setComponent(componentName);
        ThreadUtils.postOnUiThread(
                () -> {
                    try (TraceEvent event =
                            TraceEvent.scoped("IpProtectionAuthClient.Create.TryBind")) {
                        var connectionSetup = new ConnectionSetup(context, callback);
                        try {
                            boolean binding =
                                    context.bindService(
                                            intent, connectionSetup, Context.BIND_AUTO_CREATE);
                            if (!binding) {
                                connectionSetup.unbindIfBound();
                                callback.onError("bindService() failed: returned false");
                                return;
                            }
                        } catch (SecurityException e) {
                            connectionSetup.unbindIfBound();
                            callback.onError("Failed to bind service: " + e);
                            return;
                        }
                    }
                });
    }

    // See documentation in native IpProtectionAuthClient::GetInitialData
    @CalledByNative
    public void getInitialData(byte[] request, IpProtectionByteArrayCallback callback) {
        try (TraceEvent event =
                TraceEvent.scoped("IpProtectionAuthClient.Request.GetInitialData")) {
            assert mService != null;
            CallbackTracker.TrackedCallback trackedCallback =
                    mCallbackTracker.wrapCallback(callback);
            IIpProtectionGetInitialDataCallbackStub callbackStub =
                    new IIpProtectionGetInitialDataCallbackStub(trackedCallback);
            try {
                mService.getInitialData(request, callbackStub);
            } catch (RuntimeException | RemoteException ex) {
                Log.e(TAG, "error calling getInitialData", ex);
                trackedCallback.onError(AuthRequestError.OTHER);
            }
        }
    }

    // See documentation in native IpProtectionAuthClient::AuthAndSign
    @CalledByNative
    public void authAndSign(byte[] request, IpProtectionByteArrayCallback callback) {
        try (TraceEvent event = TraceEvent.scoped("IpProtectionAuthClient.Request.AuthAndSign")) {
            assert mService != null;
            CallbackTracker.TrackedCallback trackedCallback =
                    mCallbackTracker.wrapCallback(callback);
            IIpProtectionAuthAndSignCallbackStub callbackStub =
                    new IIpProtectionAuthAndSignCallbackStub(trackedCallback);
            try {
                mService.authAndSign(request, callbackStub);
            } catch (RuntimeException | RemoteException ex) {
                Log.e(TAG, "error calling authAndSign", ex);
                trackedCallback.onError(AuthRequestError.OTHER);
            }
        }
    }

    // See documentation in native IpProtectionAuthClient::GetProxyConfig
    @CalledByNative
    public void getProxyConfig(byte[] request, IpProtectionByteArrayCallback callback) {
        try (TraceEvent event =
                TraceEvent.scoped("IpProtectionAuthClient.Request.GetProxyConfig")) {
            assert mService != null;
            CallbackTracker.TrackedCallback trackedCallback =
                    mCallbackTracker.wrapCallback(callback);
            IIpProtectionGetProxyConfigCallbackStub callbackStub =
                    new IIpProtectionGetProxyConfigCallbackStub(trackedCallback);
            try {
                mService.getProxyConfig(request, callbackStub);
            } catch (RuntimeException | RemoteException ex) {
                Log.e(TAG, "error calling getProxyConfig", ex);
                trackedCallback.onError(AuthRequestError.OTHER);
            }
        }
    }

    @Override
    @CalledByNative
    public void close() {
        if (mService == null) {
            return;
        }
        assumeNonNull(mConnection);
        ThreadUtils.runOnUiThread(mConnection::unbindIfBound);
        mConnection = null;
        mService = null;
        mCallbackTracker.rejectUnresolvedCallbacks(AuthRequestError.OTHER);
    }
}
