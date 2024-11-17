// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifierAutoDetect;
import org.chromium.net.RegistrationPolicyAlwaysRegister;

/**
 * Network listener that can notify network connectivity changes based on chromium network API.
 *
 * This object lives and must be used on background threads.
 */
class BackgroundNetworkStatusListener implements NetworkChangeNotifierAutoDetect.Observer {
    private final NetworkChangeNotifierAutoDetect mNotifier;
    private static AutoDetectFactory sAutoDetectFactory = new AutoDetectFactory();

    // An observer to receive network events on main thread.
    private final Observer mObserver;
    private Handler mMainThreadHandler = new Handler(ThreadUtils.getUiThreadLooper());

    /** Observer interface to receive network change events on the main thread. */
    interface Observer {
        /**
         * Called when {@link BackgroundNetworkStatusListener} is initialized on background thread.
         */
        void onNetworkStatusReady(@ConnectionType int connectionType);

        /**
         * Called when connection type is changed.
         * @param newConnectionType The new connection type.
         */
        void onConnectionTypeChanged(@ConnectionType int newConnectionType);
    }

    /**
     * Creates the NetworkChangeNotifierAutoDetect used in this class. Included so that tests
     * can override it.
     */
    @VisibleForTesting
    public static class AutoDetectFactory {
        public NetworkChangeNotifierAutoDetect create(
                NetworkChangeNotifierAutoDetect.Observer observer,
                NetworkChangeNotifierAutoDetect.RegistrationPolicy policy) {
            return new NetworkChangeNotifierAutoDetect(observer, policy);
        }
    }

    @VisibleForTesting
    public static void setAutoDetectFactory(AutoDetectFactory factory) {
        sAutoDetectFactory = factory;
    }

    BackgroundNetworkStatusListener(Observer observer) {
        ThreadUtils.assertOnBackgroundThread();
        mObserver = observer;

        // Register policy that can fire network change events when the application is in the
        // background.
        mNotifier = sAutoDetectFactory.create(this, new RegistrationPolicyAlwaysRegister());

        // Update the connection type immediately on main thread.
        @ConnectionType int connectionType = getCurrentConnectionType();
        runOnMainThread(
                () -> {
                    mObserver.onNetworkStatusReady(connectionType);
                });
    }

    @ConnectionType
    int getCurrentConnectionType() {
        ThreadUtils.assertOnBackgroundThread();
        assert mNotifier != null;

        // TODO(crbug.com/40936429): remove this call if it is not necessary.
        mNotifier.updateCurrentNetworkState();
        return mNotifier.getCurrentNetworkState().getConnectionType();
    }

    void unRegister() {
        ThreadUtils.assertOnBackgroundThread();
        assert mNotifier != null;
        mNotifier.unregister();
    }

    private void runOnMainThread(Runnable runnable) {
        ThreadUtils.assertOnBackgroundThread();
        mMainThreadHandler.post(runnable);
    }

    /** {@link NetworkChangeNotifierAutoDetect.Observer} implementation. */
    @Override
    public void onConnectionTypeChanged(int newConnectionType) {
        runOnMainThread(
                () -> {
                    mObserver.onConnectionTypeChanged(newConnectionType);
                });
    }

    @Override
    public void onConnectionCostChanged(int newConnectionCost) {}

    @Override
    public void onConnectionSubtypeChanged(int newConnectionSubtype) {}

    @Override
    public void onNetworkConnect(long netId, int connectionType) {}

    @Override
    public void onNetworkSoonToDisconnect(long netId) {}

    @Override
    public void onNetworkDisconnect(long netId) {}

    @Override
    public void purgeActiveNetworkList(long[] activeNetIds) {}
}
