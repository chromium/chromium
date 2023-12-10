// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.os.Handler;
import android.os.HandlerThread;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.components.download.BackgroundNetworkStatusListener.Observer;
import org.chromium.net.ConnectionType;

/**
 * JNI bridge between the native network status listener and Android implementation that uses
 * network API to monitor network status.
 * This object lives on main thread, so does the native object associated with it.
 */
@JNINamespace("download")
public class NetworkStatusListenerAndroid implements BackgroundNetworkStatusListener.Observer {
    private static final String THREAD_NAME = "NetworkStatusListener";
    private long mNativePtr;
    private static Helper sSingletonHelper;

    /**
     * Helper class to query network state on one background thread. Notice that multiple
     * NetworkStatusListenerAndroid instances may access the same singleton helper.
     */
    @VisibleForTesting
    static class Helper implements BackgroundNetworkStatusListener.Observer {
        // A thread handler that |mBackgroundNetworkStatusListener| lives on, which performs actual
        // network queries. Use a background thread to avoid jank on main thread.
        private Handler mNetworkThreadHandler;

        // The object that performs actual network queries on a background thread.
        private BackgroundNetworkStatusListener mBackgroundNetworkStatusListener;

        private boolean mReady;
        private @ConnectionType int mConnectionType = ConnectionType.CONNECTION_UNKNOWN;
        private ObserverList<BackgroundNetworkStatusListener.Observer> mObservers =
                new ObserverList<>();

        Helper() {
            ThreadUtils.assertOnUiThread();
            // Creates the background thread object to listen to network change.
            HandlerThread handlerThread = new HandlerThread(THREAD_NAME);
            handlerThread.start();
            mNetworkThreadHandler = new Handler(handlerThread.getLooper());
            mNetworkThreadHandler.post(
                    () -> {
                        ThreadUtils.assertOnBackgroundThread();
                        mBackgroundNetworkStatusListener =
                                new BackgroundNetworkStatusListener(this);
                    });
        }

        void start(BackgroundNetworkStatusListener.Observer observer) {
            mObservers.addObserver(observer);

            // Make sure onNetworkStatusReady() is always called for each observer.
            if (mReady) observer.onNetworkStatusReady(mConnectionType);
        }

        void stop(BackgroundNetworkStatusListener.Observer observer) {
            mNetworkThreadHandler.post(
                    () -> {
                        mBackgroundNetworkStatusListener.unRegister();
                    });
            mObservers.removeObserver(observer);
        }

        int getCurrentConnectionType() {
            ThreadUtils.assertOnUiThread();
            return mConnectionType;
        }

        @Override
        public void onNetworkStatusReady(int connectionType) {
            ThreadUtils.assertOnUiThread();
            assert !mReady : "onNetworkStatusReady should be called only once.";
            if (mReady) return;

            mConnectionType = connectionType;
            mReady = true;
            for (Observer observer : mObservers) {
                observer.onNetworkStatusReady(connectionType);
            }
        }

        @Override
        public void onConnectionTypeChanged(int newConnectionType) {
            ThreadUtils.assertOnUiThread();
            mConnectionType = newConnectionType;
            for (Observer observer : mObservers) {
                observer.onConnectionTypeChanged(newConnectionType);
            }
        }

        public Handler getHandlerForTesting() {
            return mNetworkThreadHandler;
        }
    }

    static Helper getHelperForTesting() {
        return sSingletonHelper;
    }

    private NetworkStatusListenerAndroid(long nativePtr) {
        ThreadUtils.assertOnUiThread();
        mNativePtr = nativePtr;
        getSingletonHelper().start(this);
    }

    private Helper getSingletonHelper() {
        if (sSingletonHelper != null) return sSingletonHelper;
        sSingletonHelper = new Helper();
        return sSingletonHelper;
    }

    @VisibleForTesting
    @CalledByNative
    int getCurrentConnectionType() {
        ThreadUtils.assertOnUiThread();
        return getSingletonHelper().getCurrentConnectionType();
    }

    @CalledByNative
    private void clearNativePtr() {
        ThreadUtils.assertOnUiThread();
        getSingletonHelper().stop(this);
        mNativePtr = 0;
    }

    @VisibleForTesting
    @CalledByNative
    static NetworkStatusListenerAndroid create(long nativePtr) {
        ThreadUtils.assertOnUiThread();
        return new NetworkStatusListenerAndroid(nativePtr);
    }

    @Override
    public void onNetworkStatusReady(int connectionType) {
        ThreadUtils.assertOnUiThread();
        if (mNativePtr != 0) {
            NetworkStatusListenerAndroidJni.get()
                    .onNetworkStatusReady(
                            mNativePtr, NetworkStatusListenerAndroid.this, connectionType);
        }
    }

    @Override
    public void onConnectionTypeChanged(int newConnectionType) {
        ThreadUtils.assertOnUiThread();
        if (mNativePtr != 0) {
            NetworkStatusListenerAndroidJni.get()
                    .notifyNetworkChange(
                            mNativePtr, NetworkStatusListenerAndroid.this, newConnectionType);
        }
    }

    @NativeMethods
    interface Natives {
        void onNetworkStatusReady(
                long nativeNetworkStatusListenerAndroid,
                NetworkStatusListenerAndroid caller,
                int connectionType);

        void notifyNetworkChange(
                long nativeNetworkStatusListenerAndroid,
                NetworkStatusListenerAndroid caller,
                int connectionType);
    }
}
