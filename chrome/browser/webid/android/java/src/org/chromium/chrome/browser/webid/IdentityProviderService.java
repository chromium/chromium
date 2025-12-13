// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** This class connects to Android apps through bound services. */
@JNINamespace("content::webid")
@NullMarked
public class IdentityProviderService extends Handler implements ServiceConnection {
    private static final String TAG = "FedCMBoundService";

    // TODO(crbug.com/465181345): decide if we should use AIDL or not here,
    // rather than.defining these manually.

    // The action string that the FedCM bound service must register for in its
    // intent filter.
    private static final String FEDCM_BOUND_SERVICE_INTENT_ACTION = "org.w3.FedCM";
    // Keys for the request and reply strings in the message bundles.
    private static final String FEDCM_BOUND_SERVICE_INTENT_REQUEST = "request";
    private static final String FEDCM_BOUND_SERVICE_INTENT_REPLY = "reply";
    // The message code that the recipient can use to identify the request and
    // response message.
    private static final int MSG_FEDCM_REQUEST = 1;
    private static final int MSG_FEDCM_RESPONSE = 2;

    private long mNativeIdentityProviderService;
    private boolean mIsBound;
    private @Nullable IBinder mService;

    private IdentityProviderService(long nativeIdentityProviderService) {
        super(Looper.getMainLooper());
        mNativeIdentityProviderService = nativeIdentityProviderService;
    }

    @CalledByNative
    private static IdentityProviderService create(long nativeIdentityProviderService) {
        return new IdentityProviderService(nativeIdentityProviderService);
    }

    @CalledByNative
    private void connect(
            @JniType("std::string") String packageName,
            @JniType("std::string") String serviceName) {
        if (mIsBound) {
            Log.d(TAG, "Already bound");
            IdentityProviderServiceJni.get().onConnected(mNativeIdentityProviderService, true);
            return;
        }

        Intent intent = new Intent(FEDCM_BOUND_SERVICE_INTENT_ACTION);
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        ComponentName name = new ComponentName(packageName, serviceName);
        intent.setComponent(name);
        List<ResolveInfo> services = packageManager.queryIntentServices(intent, 0);

        Log.d(TAG, services.toString());

        if (services.isEmpty()) {
            IdentityProviderServiceJni.get().onConnected(mNativeIdentityProviderService, false);
            return;
        }

        Log.d(TAG, "Binding service");
        boolean binding = context.bindService(intent, this, Context.BIND_AUTO_CREATE);

        if (!binding) {
            Log.d(TAG, "Binding failed");
            IdentityProviderServiceJni.get().onConnected(mNativeIdentityProviderService, false);
            return;
        }
    }

    @Override
    public void onBindingDied(ComponentName name) {
        Log.d(TAG, "onBindingDied");
        mIsBound = false;
        IdentityProviderServiceJni.get().onConnected(mNativeIdentityProviderService, mIsBound);
    }

    @Override
    public void onNullBinding(ComponentName name) {
        Log.d(TAG, "onNullBinding");
        mIsBound = false;
        IdentityProviderServiceJni.get().onConnected(mNativeIdentityProviderService, mIsBound);
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        Log.d(TAG, "Connected");
        mService = service;
        mIsBound = true;
        IdentityProviderServiceJni.get().onConnected(mNativeIdentityProviderService, mIsBound);
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        Log.d(TAG, "Disconnected");
        mIsBound = false;
        IdentityProviderServiceJni.get().onDisconnected(mNativeIdentityProviderService);
    }

    @CalledByNative
    private void fetch() {
        if (!mIsBound) {
            IdentityProviderServiceJni.get().onDataFetched(mNativeIdentityProviderService, null);
            return;
        }
        Messenger serviceMessenger = new Messenger(mService);
        Message msg = Message.obtain();
        msg.what = MSG_FEDCM_REQUEST;
        Bundle bundle = new Bundle();
        bundle.putString(FEDCM_BOUND_SERVICE_INTENT_REQUEST, "Hello? ");
        msg.setData(bundle);
        Messenger responseMessenger = new Messenger(this);
        msg.replyTo = responseMessenger;
        try {
            serviceMessenger.send(msg);
        } catch (RemoteException e) {
            Log.d(TAG, "Oops remote exception: %s", e);
            IdentityProviderServiceJni.get().onDataFetched(mNativeIdentityProviderService, null);
        }
    }

    @Override
    public void handleMessage(Message msg) {
        Log.d(TAG, "Got message back");
        if (msg.what != MSG_FEDCM_RESPONSE) {
            IdentityProviderServiceJni.get().onDataFetched(mNativeIdentityProviderService, null);
            return;
        }

        String reply = msg.getData().getString(FEDCM_BOUND_SERVICE_INTENT_REPLY);
        IdentityProviderServiceJni.get().onDataFetched(mNativeIdentityProviderService, reply);
    }

    @CalledByNative
    private void disconnect() {
        if (mIsBound) {
            Context context = ContextUtils.getApplicationContext();
            context.unbindService(this);
            mIsBound = false;
        }

        if (mNativeIdentityProviderService != 0) {
            IdentityProviderServiceJni.get().onDisconnected(mNativeIdentityProviderService);
        }
    }

    @CalledByNative
    private void destroy() {
        mNativeIdentityProviderService = 0;
    }

    @NativeMethods
    interface Natives {
        void onDataFetched(
                long nativeIdentityProviderService,
                @JniType("std::optional<std::string>") @Nullable String data);

        void onConnected(long nativeIdentityProviderService, boolean success);

        void onDisconnected(long nativeIdentityProviderService);
    }
}
